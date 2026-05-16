#include "shared/theme/ThemeColorPalette.h"

#include "shared/services/ImageService.h"
#include "shared/services/AudioService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/MinecraftButton.h"

#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QLineEdit>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRegularExpression>
#include <QRegion>
#include <QResizeEvent>

namespace {

const QString kBackgroundSource(QStringLiteral(":/resources/icon/oak_planks_palette_tile.png"));
const QString kSunSource(QStringLiteral(":/resources/icon/sun.png"));
const QString kMoonSource(QStringLiteral(":/resources/icon/moon.png"));
const QString kSliderHandleSource(QStringLiteral(":/resources/icon/mc_slider_handle.png"));
const QString kSliderHandleHighlightedSource(QStringLiteral(":/resources/icon/mc_slider_handle_highlighted.png"));
const QString kSettingsFontSource(QStringLiteral(":/resources/font/MinecraftAE.ttf"));

constexpr int kPopupPadding = 18;
constexpr int kSpectrumMaxWidth = 320;
constexpr int kSpectrumMinWidth = 260;
constexpr int kSpectrumHeight = 96;
constexpr int kSpectrumMinHeight = 88;
constexpr int kPickerOuterRadius = 10;
constexpr int kPickerInnerRadius = 7;
constexpr int kIconSize = 20;
constexpr int kSliderTrackHeight = 18;
constexpr int kSliderHandleHeight = 33;
constexpr int kMinimumSliderHandleWidth = 10;
constexpr int kSpectrumSliderGap = 8;
constexpr int kSliderInputGap = 10;
constexpr int kControlGap = 6;
constexpr int kIconSliderGap = 16;
constexpr int kRowHeight = 34;
constexpr int kInputHeight = 24;
constexpr int kSampleSize = 24;
constexpr int kButtonWidth = 64;
constexpr int kButtonHeight = 34;
constexpr int kInputWidth = 132;
constexpr int kPopupWidth = 340;
constexpr int kPopupHeight = 216;
constexpr int kIconGlowSpread = 38;
constexpr int kIconGlowMaxAlpha = 150;
const QColor kSunIconGlowColor(0xffda66);
const QColor kMoonIconGlowColor(0x22304d);

QFont paletteFont(const QFont& fallback)
{
    static const QString family = [] {
        const int fontId = QFontDatabase::addApplicationFont(kSettingsFontSource);
        if (fontId < 0) {
            return QString();
        }
        const QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        return families.isEmpty() ? QString() : families.first();
    }();

    QFont font = fallback;
    if (!family.isEmpty()) {
        font.setFamily(family);
    }
    return font;
}

QString rgbToHexText(const QColor& color)
{
    return QStringLiteral("#%1%2%3")
            .arg(color.red(), 2, 16, QLatin1Char('0'))
            .arg(color.green(), 2, 16, QLatin1Char('0'))
            .arg(color.blue(), 2, 16, QLatin1Char('0'))
            .toUpper();
}

bool parseHexColor(const QString& hex, QColor* color)
{
    if (hex.size() != 6) {
        return false;
    }

    static const QRegularExpression hexPattern(QStringLiteral("^[0-9A-Fa-f]{6}$"));
    if (!hexPattern.match(hex).hasMatch()) {
        return false;
    }

    bool ok = false;
    const uint value = hex.toUInt(&ok, 16);
    if (!ok) {
        return false;
    }

    *color = QColor(static_cast<int>((value >> 16) & 0xff),
                    static_cast<int>((value >> 8) & 0xff),
                    static_cast<int>(value & 0xff));
    return true;
}

bool parseDecimalColor(const QString& text, QColor* color)
{
    const QStringList parts = text.split(QLatin1Char(','), Qt::KeepEmptyParts);
    if (parts.size() != 3) {
        return false;
    }

    int values[3] = {0, 0, 0};
    for (int i = 0; i < 3; ++i) {
        bool ok = false;
        values[i] = parts.at(i).trimmed().toInt(&ok, 10);
        if (!ok || values[i] < 0 || values[i] > 255) {
            return false;
        }
    }

    *color = QColor(values[0], values[1], values[2]);
    return true;
}

bool parseColorText(const QString& text, QColor* color)
{
    QString value = text.trimmed();
    if (value.isEmpty()) {
        return false;
    }

    if (value.contains(QLatin1Char(','))) {
        return parseDecimalColor(value, color);
    }

    if (value.startsWith(QStringLiteral("#"))) {
        value = value.mid(1);
    } else if (value.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
        value = value.mid(2);
    }

    return parseHexColor(value, color);
}

void drawIconGlow(QPainter& painter, const QRect& iconRect, qreal strength, const QColor& glowColor)
{
    const qreal boundedStrength = qBound<qreal>(0.0, strength, 1.0);
    const int centerAlpha = qRound(kIconGlowMaxAlpha * boundedStrength);
    if (centerAlpha <= 0 || iconRect.isEmpty()) {
        return;
    }

    const QRect glowRect = iconRect.adjusted(-kIconGlowSpread,
                                             -kIconGlowSpread,
                                             kIconGlowSpread,
                                             kIconGlowSpread);
    const QPointF center(iconRect.center());
    const qreal radius = iconRect.width() / 2.0 + kIconGlowSpread;

    QColor inner = glowColor;
    inner.setAlpha(centerAlpha);
    QColor middle = glowColor;
    middle.setAlpha(qRound(centerAlpha * 0.48));
    QColor outer = glowColor;
    outer.setAlpha(0);

    QRadialGradient glow(center, radius);
    glow.setColorAt(0.0, inner);
    glow.setColorAt(0.45, middle);
    glow.setColorAt(1.0, outer);

    painter.save();
    const QRegion glowRegion(glowRect);
    const QRegion iconRegion(iconRect.adjusted(-1, -1, 1, 1));
    painter.setClipRegion(glowRegion.subtracted(iconRegion), Qt::IntersectClip);
    painter.fillRect(glowRect, QBrush(glow));
    painter.restore();
}

class ColorLineEdit final : public QLineEdit
{
public:
    explicit ColorLineEdit(QWidget* parent = nullptr)
        : QLineEdit(parent)
    {
        setFrame(false);
        setAlignment(Qt::AlignCenter);
        setAutoFillBackground(false);
        setAttribute(Qt::WA_MacShowFocusRect, false);
        setTextMargins(8, 0, 8, 0);
        setMaxLength(15);

        QFont inputFont = paletteFont(font());
        inputFont.setPixelSize(14);
        inputFont.setBold(true);
        setFont(inputFont);

        QPalette p = palette();
        p.setColor(QPalette::Base, Qt::transparent);
        p.setColor(QPalette::Text, QColor(0xff, 0xff, 0xff));
        p.setColor(QPalette::Highlight, QColor(0x00, 0x99, 0xff));
        p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
        setPalette(p);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, false);

        const QRect borderRect = rect().adjusted(0, 1, -1, -2);
        painter.fillRect(borderRect, QColor(25, 25, 25, 215));
        painter.setPen(QColor(0, 0, 0, 230));
        painter.drawRect(borderRect);
        painter.setPen(hasFocus() ? QColor(255, 255, 255, 190) : QColor(255, 255, 255, 90));
        painter.drawRect(borderRect.adjusted(1, 1, -1, -1));

        QLineEdit::paintEvent(event);
    }
};

} // namespace

ThemeColorPalette::ThemeColorPalette(QWidget* parent)
    : QWidget(parent)
    , m_input(new ColorLineEdit(this))
    , m_confirmButton(new MinecraftButton(QStringLiteral("确认"), this))
    , m_cancelButton(new MinecraftButton(QStringLiteral("取消"), this))
{
    setWindowFlags(Qt::Popup | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setFont(paletteFont(font()));
    m_confirmButton->setFixedSize(kButtonWidth, kButtonHeight);
    m_cancelButton->setFixedSize(kButtonWidth, kButtonHeight);
    setFixedSize(sizeHint());
    layoutControls();

    connect(m_input, &QLineEdit::editingFinished, this, [this]() {
        applyInputText();
    });
    connect(m_confirmButton, &QPushButton::clicked, this, &ThemeColorPalette::acceptSelection);
    connect(m_cancelButton, &QPushButton::clicked, this, &ThemeColorPalette::cancelSelection);

    updateInputText();
}

QColor ThemeColorPalette::currentColor() const
{
    return m_color;
}

QSize ThemeColorPalette::sizeHint() const
{
    return {kPopupWidth, kPopupHeight};
}

QSize ThemeColorPalette::minimumSizeHint() const
{
    return {kPopupWidth, kPopupHeight};
}

void ThemeColorPalette::setCurrentColor(const QColor& color)
{
    if (!color.isValid()) {
        return;
    }

    setColorFromRgb(color, true);
}

bool ThemeColorPalette::event(QEvent* event)
{
    if (event->type() == QEvent::Leave || event->type() == QEvent::HoverLeave) {
        if (!m_draggingBrightness) {
            m_sliderHandleHovered = false;
            update(sliderHandleRect().adjusted(-2, -2, 2, 2));
        }
    }

    return QWidget::event(event);
}

void ThemeColorPalette::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        cancelSelection();
        event->accept();
        return;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (m_input && m_input->hasFocus()) {
            applyInputText();
            event->accept();
            return;
        }
        if (applyInputText()) {
            acceptSelection();
        }
        event->accept();
        return;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
}

void ThemeColorPalette::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const QPoint point = event->position().toPoint();
    if (spectrumRect().contains(point)) {
        m_draggingSpectrum = true;
        setColorFromSpectrumPosition(point);
        event->accept();
        return;
    }

    if (sliderTrackRect().adjusted(-10, -8, 10, 8).contains(point)
        || sliderHandleRect().contains(point)) {
        m_draggingBrightness = true;
        m_sliderHandleHovered = true;
        setBrightnessFromPosition(point.x());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ThemeColorPalette::mouseMoveEvent(QMouseEvent* event)
{
    const QPoint point = event->position().toPoint();
    if (m_draggingSpectrum) {
        setColorFromSpectrumPosition(point);
        event->accept();
        return;
    }

    if (m_draggingBrightness) {
        setBrightnessFromPosition(point.x());
        event->accept();
        return;
    }

    const bool hovered = sliderHandleRect().contains(point)
                         || sliderTrackRect().adjusted(-10, -8, 10, 8).contains(point);
    if (m_sliderHandleHovered != hovered) {
        m_sliderHandleHovered = hovered;
        update(sliderHandleRect().adjusted(-2, -2, 2, 2));
    }

    QWidget::mouseMoveEvent(event);
}

void ThemeColorPalette::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && (m_draggingSpectrum || m_draggingBrightness)) {
        const bool wasDraggingBrightness = m_draggingBrightness;
        m_draggingSpectrum = false;
        m_draggingBrightness = false;
        m_sliderHandleHovered = sliderHandleRect().contains(event->position().toPoint());
        if (wasDraggingBrightness) {
            AudioService::instance().playButtonClick();
        }
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void ThemeColorPalette::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, false);

    painter.fillRect(rect(), QColor(0, 0, 0));
    const QPixmap background = ImageService::instance().pixmap(kBackgroundSource);
    if (background.isNull()) {
        painter.fillRect(rect(), QColor(0x2f, 0x24, 0x18));
    } else {
        painter.save();
        painter.setOpacity(0.52);
        painter.drawTiledPixmap(rect(), background);
        painter.restore();
    }
    painter.fillRect(rect(), QColor(0, 0, 0, 44));
    painter.setPen(QPen(QColor(0, 0, 0, 235), 3));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(rect().adjusted(1, 1, -2, -2));
    painter.setPen(QPen(QColor(255, 255, 255, 80), 1));
    painter.drawRect(rect().adjusted(4, 4, -5, -5));

    const QRect spectrum = spectrumRect();
    rebuildSpectrumCache();
    const QRectF spectrumArea(spectrum);
    QPainterPath spectrumClip;
    spectrumClip.addRoundedRect(spectrumArea, 5, 5);
    painter.setClipPath(spectrumClip);
    if (!m_spectrumCache.isNull()) {
        painter.drawImage(spectrum, m_spectrumCache);
    } else {
        painter.fillRect(spectrum, m_color);
    }
    painter.setClipping(false);
    painter.setPen(QPen(QColor(0, 0, 0, 230), 1));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(spectrumArea.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);
    painter.setPen(QPen(QColor(255, 255, 255, 115), 1));
    painter.drawRoundedRect(spectrumArea.adjusted(2.5, 2.5, -2.5, -2.5), 3, 3);

    const QPoint pickerCenter = spectrumHandleCenter();
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(pickerCenter, kPickerOuterRadius, kPickerOuterRadius);
    painter.setBrush(m_color);
    painter.drawEllipse(pickerCenter, kPickerInnerRadius, kPickerInnerRadius);

    const QRect track = sliderTrackRect();
    const QRect sun = sunRect();
    const QRect moon = moonRect();
    const QPixmap sunPixmap = ImageService::instance().pixmap(kSunSource);
    const QPixmap moonPixmap = ImageService::instance().pixmap(kMoonSource);
    painter.save();
    painter.setClipRect(QRect(0, spectrum.bottom() + 1, width(), height() - spectrum.bottom() - 1));
    drawIconGlow(painter, sun, m_value, kSunIconGlowColor);
    drawIconGlow(painter, moon, 1.0 - m_value, kMoonIconGlowColor);
    painter.restore();
    if (sunPixmap.isNull()) {
        painter.setBrush(QColor(255, 222, 90));
        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawEllipse(sun);
    } else {
        painter.drawPixmap(sun, sunPixmap, sunPixmap.rect());
    }
    if (moonPixmap.isNull()) {
        painter.setBrush(QColor(185, 195, 220));
        painter.setPen(QColor(0, 0, 0, 180));
        painter.drawEllipse(moon);
    } else {
        painter.drawPixmap(moon, moonPixmap, moonPixmap.rect());
    }

    QLinearGradient gradient(track.topLeft(), track.topRight());
    gradient.setColorAt(0.0, QColor::fromHsvF(m_hue, m_saturation, 1.0));
    gradient.setColorAt(1.0, QColor(0, 0, 0));
    painter.fillRect(track, gradient);
    painter.setPen(QPen(QColor(0, 0, 0, 240), 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawRect(track.adjusted(0, 0, -1, -1));

    const QRect handle = sliderHandleRect();
    const QString handleSource = (m_sliderHandleHovered || m_draggingBrightness)
            ? kSliderHandleHighlightedSource
            : kSliderHandleSource;
    const QPixmap handlePixmap = ImageService::instance().pixmap(handleSource);
    if (handlePixmap.isNull()) {
        painter.fillRect(handle, QColor(0xc6, 0xc6, 0xc6));
        painter.setPen(QColor(0, 0, 0, 220));
        painter.drawRect(handle.adjusted(0, 0, -1, -1));
    } else {
        painter.drawPixmap(handle, handlePixmap, handlePixmap.rect());
    }

    const QRect swatch = sampleRect();
    const QPointF swatchCenter(swatch.left() + swatch.width() / 2.0,
                               swatch.top() + swatch.height() / 2.0);
    painter.setPen(QPen(m_color, 2));
    painter.setBrush(Qt::NoBrush);
    painter.drawEllipse(swatchCenter, 10.5, 10.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::white);
    painter.drawEllipse(swatchCenter, 9.5, 9.5);
    painter.setBrush(m_color);
    painter.drawEllipse(swatchCenter, 7.0, 7.0);
}

void ThemeColorPalette::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutControls();
    m_cachedSpectrumSize = {};
}

void ThemeColorPalette::layoutControls()
{
    const QRect spectrum = spectrumRect();
    const QRect track = sliderTrackRect();
    const int rowTop = qMax(track.bottom(), sliderHandleRect().bottom()) + kSliderInputGap;
    const int rowLeft = spectrum.left();
    const int rowRight = spectrum.right();

    m_sampleRect = QRect(rowLeft, rowTop + (kRowHeight - kSampleSize) / 2, kSampleSize, kSampleSize);

    int cancelX = rowRight - kButtonWidth + 1;
    int confirmX = cancelX - kControlGap - kButtonWidth;
    int inputX = m_sampleRect.right() + kControlGap + 1;
    int inputRight = confirmX - kControlGap - 1;

    if (inputRight - inputX + 1 < kInputWidth) {
        inputRight = qMin(rowRight, inputX + kInputWidth - 1);
        confirmX = rowLeft;
        cancelX = confirmX + kButtonWidth + kControlGap;
        const int buttonTop = rowTop + kRowHeight + 12;
        m_cancelButton->setGeometry(confirmX, buttonTop, kButtonWidth, kButtonHeight);
        m_confirmButton->setGeometry(cancelX, buttonTop, kButtonWidth, kButtonHeight);
    } else {
        m_cancelButton->setGeometry(confirmX, rowTop, kButtonWidth, kButtonHeight);
        m_confirmButton->setGeometry(cancelX, rowTop, kButtonWidth, kButtonHeight);
    }

    m_input->setGeometry(inputX,
                         rowTop + (kRowHeight - kInputHeight) / 2,
                         qBound(kInputWidth, inputRight - inputX + 1, kInputWidth),
                         kInputHeight);
}

void ThemeColorPalette::rebuildSpectrumCache()
{
    const QRect spectrum = spectrumRect();
    const QSize targetSize = spectrum.size();
    if (targetSize.isEmpty()) {
        m_spectrumCache = {};
        return;
    }

    if (m_cachedSpectrumSize == targetSize && qFuzzyCompare(m_cachedValue, m_value)
        && !m_spectrumCache.isNull()) {
        return;
    }

    QImage image(targetSize, QImage::Format_RGB32);
    const qreal widthDenominator = qMax(1, targetSize.width() - 1);
    const qreal heightDenominator = qMax(1, targetSize.height() - 1);

    for (int y = 0; y < targetSize.height(); ++y) {
        const qreal saturation = 1.0 - static_cast<qreal>(y) / heightDenominator;
        QRgb* line = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < targetSize.width(); ++x) {
            const qreal hue = static_cast<qreal>(x) / widthDenominator;
            line[x] = QColor::fromHsvF(hue, saturation, m_value).rgb();
        }
    }

    m_spectrumCache = image;
    m_cachedSpectrumSize = targetSize;
    m_cachedValue = m_value;
}

void ThemeColorPalette::setColorFromHsv(qreal hue, qreal saturation, qreal value, bool updateInput)
{
    m_hue = qBound<qreal>(0.0, hue, 1.0);
    m_saturation = qBound<qreal>(0.0, saturation, 1.0);
    m_value = qBound<qreal>(0.0, value, 1.0);
    m_color = QColor::fromHsvF(m_hue, m_saturation, m_value);
    if (updateInput) {
        updateInputText();
    }
    update();
}

void ThemeColorPalette::setColorFromRgb(const QColor& color, bool updateInput)
{
    QColor rgb = color.toRgb();
    rgb.setAlpha(255);

    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    rgb.getHsvF(&hue, &saturation, &value);
    if (hue < 0.0) {
        hue = static_cast<float>(m_hue);
    }

    setColorFromHsv(hue, saturation, value, updateInput);
}

void ThemeColorPalette::setColorFromSpectrumPosition(const QPoint& point)
{
    const QRect spectrum = spectrumRect();
    if (spectrum.isEmpty()) {
        return;
    }

    const int x = qBound(spectrum.left(), point.x(), spectrum.right());
    const int y = qBound(spectrum.top(), point.y(), spectrum.bottom());
    const qreal hue = static_cast<qreal>(x - spectrum.left()) / qMax<qreal>(1.0, spectrum.width() - 1.0);
    const qreal saturation = 1.0 - static_cast<qreal>(y - spectrum.top())
            / qMax<qreal>(1.0, spectrum.height() - 1.0);
    setColorFromHsv(hue, saturation, m_value, true);
}

void ThemeColorPalette::setBrightnessFromPosition(int x)
{
    const QRect track = sliderTrackRect();
    if (track.isEmpty()) {
        return;
    }

    const int boundedX = qBound(track.left(), x, track.right());
    const qreal darkProgress = static_cast<qreal>(boundedX - track.left())
            / qMax<qreal>(1.0, track.width() - 1.0);
    setColorFromHsv(m_hue, m_saturation, 1.0 - darkProgress, true);
}

void ThemeColorPalette::updateInputText()
{
    if (!m_input) {
        return;
    }

    const QSignalBlocker blocker(m_input);
    m_input->setText(normalizedColorText());
}

bool ThemeColorPalette::applyInputText()
{
    QColor parsed;
    if (!parseColorText(m_input->text(), &parsed)) {
        updateInputText();
        return false;
    }

    setColorFromRgb(parsed, true);
    return true;
}

void ThemeColorPalette::acceptSelection()
{
    if (!applyInputText()) {
        return;
    }
    emit accepted(m_color);
}

void ThemeColorPalette::cancelSelection()
{
    emit canceled();
}

QRect ThemeColorPalette::spectrumRect() const
{
    const int horizontalMargin = kPopupPadding;
    const int availableWidth = qMax(kSpectrumMinWidth, width() - horizontalMargin * 2);
    const int spectrumWidth = qBound(kSpectrumMinWidth, availableWidth, kSpectrumMaxWidth);
    const int spectrumHeight = qBound(kSpectrumMinHeight,
                                      qRound(spectrumWidth * 0.30),
                                      kSpectrumHeight);
    const int totalHeight = spectrumHeight + kSpectrumSliderGap + kSliderHandleHeight + kSliderInputGap + kRowHeight;
    const int top = qMax(kPopupPadding, (height() - totalHeight) / 2);
    const int left = (width() - spectrumWidth) / 2;

    m_spectrumRect = QRect(left, top, spectrumWidth, spectrumHeight);
    return m_spectrumRect;
}

QRect ThemeColorPalette::sunRect() const
{
    const QRect spectrum = spectrumRect();
    const int top = spectrum.bottom() + kSpectrumSliderGap + (kSliderHandleHeight - kIconSize) / 2;
    m_sunRect = QRect(spectrum.left(), top, kIconSize, kIconSize);
    return m_sunRect;
}

QRect ThemeColorPalette::moonRect() const
{
    const QRect spectrum = spectrumRect();
    const int top = spectrum.bottom() + kSpectrumSliderGap + (kSliderHandleHeight - kIconSize) / 2;
    m_moonRect = QRect(spectrum.right() - kIconSize + 1, top, kIconSize, kIconSize);
    return m_moonRect;
}

QRect ThemeColorPalette::sliderTrackRect() const
{
    const QRect sun = sunRect();
    const QRect moon = moonRect();
    const int left = sun.right() + kIconSliderGap + 1;
    const int right = moon.left() - kIconSliderGap - 1;
    const int top = sun.center().y() - kSliderTrackHeight / 2;
    m_sliderTrackRect = QRect(QPoint(left, top), QPoint(right, top + kSliderTrackHeight - 1));
    return m_sliderTrackRect;
}

QRect ThemeColorPalette::sliderHandleRect() const
{
    const QRect track = sliderTrackRect();
    const QPixmap handlePixmap = ImageService::instance().pixmap(kSliderHandleSource);
    int handleWidth = kMinimumSliderHandleWidth;
    if (!handlePixmap.isNull() && handlePixmap.height() > 0) {
        handleWidth = qRound(static_cast<qreal>(handlePixmap.width()) * kSliderHandleHeight
                             / static_cast<qreal>(handlePixmap.height()));
    }

    const qreal darkProgress = 1.0 - m_value;
    const int centerX = track.left() + qRound(darkProgress * qMax(0, track.width() - 1));
    return QRect(centerX - handleWidth / 2,
                 track.center().y() - kSliderHandleHeight / 2,
                 handleWidth,
                 kSliderHandleHeight);
}

QRect ThemeColorPalette::sampleRect() const
{
    return m_sampleRect;
}

QPoint ThemeColorPalette::spectrumHandleCenter() const
{
    const QRect spectrum = spectrumRect();
    const int x = spectrum.left() + qRound(m_hue * qMax(0, spectrum.width() - 1));
    const int y = spectrum.top() + qRound((1.0 - m_saturation) * qMax(0, spectrum.height() - 1));
    return {x, y};
}

QString ThemeColorPalette::normalizedColorText() const
{
    return rgbToHexText(m_color);
}
