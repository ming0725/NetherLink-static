#pragma once

#include <QImage>
#include <QtGlobal>

#include <vector>

class FastGaussianBlur
{
public:
    explicit FastGaussianBlur(int passCount = 3);

    void setPassCount(int passCount);
    int passCount() const;

    QImage blur(const QImage& source, qreal radius);

private:
    static std::vector<int> boxRadiiForGaussian(qreal sigma, int passCount);
    static void transpose(const quint32* source, quint32* destination, int width, int height);
    static quint32 averagedPixel(quint64 alpha,
                                 quint64 red,
                                 quint64 green,
                                 quint64 blue,
                                 quint64 count);

    void ensureCapacity(int pixelCount);
    void copyFromImage(const QImage& image);
    QImage copyToImage(const QSize& size, qreal devicePixelRatio) const;
    void boxBlurHorizontal(const quint32* source,
                           quint32* destination,
                           int width,
                           int height,
                           int radius);

    int m_passCount = 3;
    int m_width = 0;
    int m_height = 0;
    std::vector<quint32> m_pixels;
    std::vector<quint32> m_workA;
    std::vector<quint32> m_workB;
    std::vector<quint64> m_prefix;
};
