#include "FastGaussianBlur.h"

#include <QSize>

#include <algorithm>
#include <cmath>
#include <cstring>

namespace {

constexpr int kTransposeBlockSize = 32;

inline quint64 pixelAlpha(quint32 pixel)
{
    return (pixel >> 24) & 0xffu;
}

inline quint64 pixelRed(quint32 pixel)
{
    return (pixel >> 16) & 0xffu;
}

inline quint64 pixelGreen(quint32 pixel)
{
    return (pixel >> 8) & 0xffu;
}

inline quint64 pixelBlue(quint32 pixel)
{
    return pixel & 0xffu;
}

} // namespace

FastGaussianBlur::FastGaussianBlur(int passCount)
{
    setPassCount(passCount);
}

void FastGaussianBlur::setPassCount(int passCount)
{
    m_passCount = qMax(1, passCount);
}

int FastGaussianBlur::passCount() const
{
    return m_passCount;
}

QImage FastGaussianBlur::blur(const QImage& source, qreal radius)
{
    if (source.isNull() || source.width() <= 0 || source.height() <= 0) {
        return {};
    }

    QImage image = source.convertToFormat(QImage::Format_ARGB32_Premultiplied);
    image.setDevicePixelRatio(source.devicePixelRatio());

    m_width = image.width();
    m_height = image.height();
    const int pixelCount = m_width * m_height;
    ensureCapacity(pixelCount);
    copyFromImage(image);

    if (radius <= 0.0) {
        return image;
    }

    const std::vector<int> radii = boxRadiiForGaussian(radius, m_passCount);
    for (int boxRadius : radii) {
        if (boxRadius <= 0) {
            continue;
        }

        boxBlurHorizontal(m_pixels.data(), m_workA.data(), m_width, m_height, boxRadius);
        transpose(m_workA.data(), m_workB.data(), m_width, m_height);
        boxBlurHorizontal(m_workB.data(), m_workA.data(), m_height, m_width, boxRadius);
        transpose(m_workA.data(), m_pixels.data(), m_height, m_width);
    }

    return copyToImage(image.size(), image.devicePixelRatio());
}

std::vector<int> FastGaussianBlur::boxRadiiForGaussian(qreal sigma, int passCount)
{
    const int count = qMax(1, passCount);
    std::vector<int> radii;
    radii.reserve(static_cast<size_t>(count));

    if (sigma <= 0.0) {
        radii.assign(static_cast<size_t>(count), 0);
        return radii;
    }

    const double variance = static_cast<double>(sigma) * static_cast<double>(sigma);
    const double idealWidth = std::sqrt((12.0 * variance / count) + 1.0);
    int lowerWidth = static_cast<int>(std::floor(idealWidth));
    if (lowerWidth % 2 == 0) {
        --lowerWidth;
    }
    lowerWidth = qMax(1, lowerWidth);

    const int upperWidth = lowerWidth + 2;
    const double lowerCountIdeal =
            (12.0 * variance
             - count * lowerWidth * lowerWidth
             - 4.0 * count * lowerWidth
             - 3.0 * count)
            / (-4.0 * lowerWidth - 4.0);
    const int lowerCount = qBound(0,
                                  static_cast<int>(std::round(lowerCountIdeal)),
                                  count);

    for (int index = 0; index < count; ++index) {
        const int width = index < lowerCount ? lowerWidth : upperWidth;
        radii.push_back((width - 1) / 2);
    }
    return radii;
}

void FastGaussianBlur::ensureCapacity(int pixelCount)
{
    const size_t capacity = static_cast<size_t>(qMax(0, pixelCount));
    m_pixels.resize(capacity);
    m_workA.resize(capacity);
    m_workB.resize(capacity);
}

void FastGaussianBlur::copyFromImage(const QImage& image)
{
    for (int y = 0; y < m_height; ++y) {
        const auto* sourceLine = reinterpret_cast<const quint32*>(image.constScanLine(y));
        std::memcpy(m_pixels.data() + static_cast<size_t>(y) * m_width,
                    sourceLine,
                    static_cast<size_t>(m_width) * sizeof(quint32));
    }
}

QImage FastGaussianBlur::copyToImage(const QSize& size, qreal devicePixelRatio) const
{
    QImage output(size, QImage::Format_ARGB32_Premultiplied);
    output.setDevicePixelRatio(devicePixelRatio);

    for (int y = 0; y < m_height; ++y) {
        auto* destinationLine = reinterpret_cast<quint32*>(output.scanLine(y));
        std::memcpy(destinationLine,
                    m_pixels.data() + static_cast<size_t>(y) * m_width,
                    static_cast<size_t>(m_width) * sizeof(quint32));
    }
    return output;
}

void FastGaussianBlur::boxBlurHorizontal(const quint32* source,
                                         quint32* destination,
                                         int width,
                                         int height,
                                         int radius)
{
    if (width <= 0 || height <= 0) {
        return;
    }
    if (radius <= 0) {
        std::memcpy(destination,
                    source,
                    static_cast<size_t>(width) * height * sizeof(quint32));
        return;
    }

    const int prefixLength = width + 1;
    m_prefix.resize(static_cast<size_t>(prefixLength) * 4);
    quint64* alphaPrefix = m_prefix.data();
    quint64* redPrefix = alphaPrefix + prefixLength;
    quint64* greenPrefix = redPrefix + prefixLength;
    quint64* bluePrefix = greenPrefix + prefixLength;
    const quint64 kernelSize = static_cast<quint64>(radius) * 2u + 1u;

    for (int y = 0; y < height; ++y) {
        const quint32* sourceLine = source + static_cast<size_t>(y) * width;
        quint32* destinationLine = destination + static_cast<size_t>(y) * width;

        alphaPrefix[0] = 0;
        redPrefix[0] = 0;
        greenPrefix[0] = 0;
        bluePrefix[0] = 0;

        for (int x = 0; x < width; ++x) {
            const quint32 pixel = sourceLine[x];
            alphaPrefix[x + 1] = alphaPrefix[x] + pixelAlpha(pixel);
            redPrefix[x + 1] = redPrefix[x] + pixelRed(pixel);
            greenPrefix[x + 1] = greenPrefix[x] + pixelGreen(pixel);
            bluePrefix[x + 1] = bluePrefix[x] + pixelBlue(pixel);
        }

        const quint32 leftPixel = sourceLine[0];
        const quint32 rightPixel = sourceLine[width - 1];
        const quint64 leftAlpha = pixelAlpha(leftPixel);
        const quint64 leftRed = pixelRed(leftPixel);
        const quint64 leftGreen = pixelGreen(leftPixel);
        const quint64 leftBlue = pixelBlue(leftPixel);
        const quint64 rightAlpha = pixelAlpha(rightPixel);
        const quint64 rightRed = pixelRed(rightPixel);
        const quint64 rightGreen = pixelGreen(rightPixel);
        const quint64 rightBlue = pixelBlue(rightPixel);

        for (int x = 0; x < width; ++x) {
            const int left = qMax(0, x - radius);
            const int right = qMin(width - 1, x + radius);
            const quint64 leftRepeat = static_cast<quint64>(qMax(0, radius - x));
            const quint64 rightRepeat = static_cast<quint64>(qMax(0, x + radius - (width - 1)));

            const quint64 alpha = alphaPrefix[right + 1] - alphaPrefix[left]
                    + leftRepeat * leftAlpha
                    + rightRepeat * rightAlpha;
            const quint64 red = redPrefix[right + 1] - redPrefix[left]
                    + leftRepeat * leftRed
                    + rightRepeat * rightRed;
            const quint64 green = greenPrefix[right + 1] - greenPrefix[left]
                    + leftRepeat * leftGreen
                    + rightRepeat * rightGreen;
            const quint64 blue = bluePrefix[right + 1] - bluePrefix[left]
                    + leftRepeat * leftBlue
                    + rightRepeat * rightBlue;

            destinationLine[x] = averagedPixel(alpha, red, green, blue, kernelSize);
        }
    }
}

quint32 FastGaussianBlur::averagedPixel(quint64 alpha,
                                        quint64 red,
                                        quint64 green,
                                        quint64 blue,
                                        quint64 count)
{
    const quint64 half = count / 2u;
    const quint32 averagedAlpha = static_cast<quint32>((alpha + half) / count);
    const quint32 averagedRed = static_cast<quint32>((red + half) / count);
    const quint32 averagedGreen = static_cast<quint32>((green + half) / count);
    const quint32 averagedBlue = static_cast<quint32>((blue + half) / count);

    return (averagedAlpha << 24)
            | (averagedRed << 16)
            | (averagedGreen << 8)
            | averagedBlue;
}

void FastGaussianBlur::transpose(const quint32* source,
                                 quint32* destination,
                                 int width,
                                 int height)
{
    for (int yBlock = 0; yBlock < height; yBlock += kTransposeBlockSize) {
        const int yEnd = qMin(height, yBlock + kTransposeBlockSize);
        for (int xBlock = 0; xBlock < width; xBlock += kTransposeBlockSize) {
            const int xEnd = qMin(width, xBlock + kTransposeBlockSize);
            for (int y = yBlock; y < yEnd; ++y) {
                const quint32* sourceLine = source + static_cast<size_t>(y) * width;
                for (int x = xBlock; x < xEnd; ++x) {
                    destination[static_cast<size_t>(x) * height + y] = sourceLine[x];
                }
            }
        }
    }
}
