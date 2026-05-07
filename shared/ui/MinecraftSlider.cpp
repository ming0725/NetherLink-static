#include "MinecraftSlider.h"

#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"

#include <QEvent>
#include <QFont>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>

namespace {

const QString kDefaultSliderImage(QStringLiteral(":/resources/icon/mc_slider.png"));
const QString kDefaultHandleImage(QStringLiteral(":/resources/icon/mc_slider_handle.png"));
const QString kDefaultHandleHoverImage(QStringLiteral(":/resources/icon/mc_slider_handle_highlighted.png"));
constexpr int kDefaultSliderWidth = 192;
constexpr int kDefaultSliderHeight = 40;
constexpr int kMinimumSliderWidth = 96;
constexpr int kMinimumHandleWidth = 10;

QMargins scaledTargetMargins(const QSize& sourceSize, const QSize& targetSize, const QMargins& sourceMargins)
{
    if (sourceSize.isEmpty() || targetSize.isEmpty()) {
        return {};
    }

    const qreal scale = static_cast<qreal>(targetSize.height()) / static_cast<qreal>(sourceSize.height());
    int left = qMax(1, qRound(sourceMargins.left() * scale));
    int right = qMax(1, qRound(sourceMargins.right() * scale));
    int top = qMax(1, qRound(sourceMargins.top() * scale));
    int bottom = qMax(1, qRound(sourceMargins.bottom() * scale));

    const int maxHorizontalMargin = qMax(1, targetSize.width() / 2 - 1);
    const int maxVerticalMargin = qMax(1, targetSize.height() / 2 - 1);
    left = qMin(left, maxHorizontalMargin);
    right = qMin(right, maxHorizontalMargin);
    top = qMin(top, maxVerticalMargin);
    bottom = qMin(bottom, maxVerticalMargin);

    return {left, top, right, bottom};
}

} // namespace

MinecraftSlider::MinecraftSlider(QWidget* parent)
    : QWidget(parent)
    , m_normalImage(kDefaultSliderImage)
    , m_handleImage(kDefaultHandleImage)
    , m_handleHoverImage(kDefaultHandleHoverImage)
    , m_sourceMargins(5, 5, 5, 5)
{
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(minimumSizeHint());
}

void MinecraftSlider::setRange(int minimum, int maximum)
{
    if (minimum > maximum) {
        qSwap(minimum, maximum);
    }

    if (m_minimum == minimum && m_maximum == maximum) {
        return;
    }

    m_minimum = minimum;
    m_maximum = maximum;
    setValue(m_value);
    update();
}

void MinecraftSlider::setValue(int value)
{
    const int boundedValue = qBound(m_minimum, value, m_maximum);
    if (m_value == boundedValue) {
        return;
    }

    m_value = boundedValue;
    update();
    emit valueChanged(m_value);
}

void MinecraftSlider::setNormalImage(const QString& source)
{
    if (m_normalImage == source) {
        return;
    }

    m_normalImage = source;
    update();
}

void MinecraftSlider::setHandleImage(const QString& source)
{
    if (m_handleImage == source) {
        return;
    }

    m_handleImage = source;
    update();
}

void MinecraftSlider::setHandleHoverImage(const QString& source)
{
    if (m_handleHoverImage == source) {
        return;
    }

    m_handleHoverImage = source;
    update();
}

void MinecraftSlider::setSourceMargins(const QMargins& margins)
{
    if (m_sourceMargins == margins) {
        return;
    }

    m_sourceMargins = margins;
    update();
}

void MinecraftSlider::setText(const QString& text)
{
    if (m_text == text) {
        return;
    }

    m_text = text;
    update();
}

int MinecraftSlider::value() const
{
    return m_value;
}

QSize MinecraftSlider::sizeHint() const
{
    return {kDefaultSliderWidth, kDefaultSliderHeight};
}

QSize MinecraftSlider::minimumSizeHint() const
{
    return {kMinimumSliderWidth, kDefaultSliderHeight};
}

bool MinecraftSlider::event(QEvent* event)
{
    switch (event->type()) {
    case QEvent::Enter:
    case QEvent::HoverEnter:
        setHovered(true);
        break;
    case QEvent::Leave:
    case QEvent::HoverLeave:
    case QEvent::Hide:
    case QEvent::WindowDeactivate:
        if (!m_dragging) {
            setHovered(false);
        }
        break;
    case QEvent::EnabledChange:
        m_dragging = false;
        setHovered(false);
        break;
    default:
        break;
    }

    return QWidget::event(event);
}

void MinecraftSlider::mousePressEvent(QMouseEvent* event)
{
    if (!isEnabled() || event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_dragging = true;
    setHovered(true);
    setValue(valueFromPosition(event->position().toPoint().x()));
    event->accept();
}

void MinecraftSlider::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_dragging) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    setValue(valueFromPosition(event->position().toPoint().x()));
    event->accept();
}

void MinecraftSlider::mouseReleaseEvent(QMouseEvent* event)
{
    if (m_dragging && event->button() == Qt::LeftButton) {
        m_dragging = false;
        setHovered(rect().contains(event->position().toPoint()));
        AudioService::instance().playButtonClick();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

void MinecraftSlider::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPixmap sliderPixmap = ImageService::instance().pixmap(m_normalImage);
    if (sliderPixmap.isNull()) {
        painter.fillRect(rect(), QColor(0x75, 0x75, 0x75));
    } else {
        drawNinePatch(painter, sliderPixmap, rect());
    }

    const QString handleSource = (isEnabled() && (m_hovered || m_dragging)) ? m_handleHoverImage : m_handleImage;
    const QPixmap handlePixmap = ImageService::instance().pixmap(handleSource);
    const QRect handle = handleRect();
    if (handlePixmap.isNull()) {
        painter.fillRect(handle, isEnabled() ? QColor(0xC6, 0xC6, 0xC6) : QColor(0x90, 0x90, 0x90));
    } else {
        painter.drawPixmap(handle, handlePixmap, handlePixmap.rect());
    }

    if (!m_text.isEmpty()) {
        QFont textFont = font();
        textFont.setBold(true);
        textFont.setPixelSize(13);
        painter.setFont(textFont);

        const QRect textRect = rect().adjusted(8, 0, -8, 0);
        painter.setPen(QColor(0, 0, 0, isEnabled() ? 180 : 110));
        painter.drawText(textRect.translated(1, 1), Qt::AlignCenter, m_text);
        painter.setPen(isEnabled() ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xB0, 0xB0, 0xB0));
        painter.drawText(textRect, Qt::AlignCenter, m_text);
    }

    if (!isEnabled()) {
        painter.fillRect(rect(), QColor(0, 0, 0, 90));
    }
}

QRect MinecraftSlider::handleRect() const
{
    const int widthForHandle = handleWidth();
    const int availableWidth = qMax(0, width() - widthForHandle);
    const int range = qMax(1, m_maximum - m_minimum);
    const qreal progress = static_cast<qreal>(m_value - m_minimum) / static_cast<qreal>(range);
    const int x = qRound(progress * availableWidth);

    return {x, 0, widthForHandle, height()};
}

int MinecraftSlider::handleWidth() const
{
    const QPixmap handlePixmap = ImageService::instance().pixmap(m_handleImage);
    if (handlePixmap.isNull() || handlePixmap.height() <= 0) {
        return qMax(kMinimumHandleWidth, height() / 2);
    }

    const int scaledWidth = qRound(static_cast<qreal>(handlePixmap.width()) * height()
                                   / static_cast<qreal>(handlePixmap.height()));
    return qBound(kMinimumHandleWidth, scaledWidth, qMax(kMinimumHandleWidth, width()));
}

int MinecraftSlider::valueFromPosition(int x) const
{
    const int widthForHandle = handleWidth();
    const int availableWidth = qMax(1, width() - widthForHandle);
    const int range = qMax(0, m_maximum - m_minimum);
    if (range == 0) {
        return m_minimum;
    }

    const int handleX = qBound(0, x - widthForHandle / 2, availableWidth);
    const int valueCount = range + 1;
    const int handleSlots = qMax(1, availableWidth / qMax(1, widthForHandle)) + 1;

    if (valueCount <= handleSlots) {
        const qreal stepWidth = static_cast<qreal>(availableWidth) / static_cast<qreal>(range);
        const int snappedIndex = qBound(0, qRound(handleX / stepWidth), range);
        return m_minimum + snappedIndex;
    }

    const qreal progress = static_cast<qreal>(handleX) / static_cast<qreal>(availableWidth);
    return m_minimum + qBound(0, qRound(progress * range), range);
}

void MinecraftSlider::setHovered(bool hovered)
{
    if (!isEnabled()) {
        hovered = false;
    }

    if (m_hovered == hovered) {
        return;
    }

    m_hovered = hovered;
    update();
}

void MinecraftSlider::drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const
{
    if (pixmap.isNull() || targetRect.isEmpty()) {
        return;
    }

    const QRect sourceRect(QPoint(0, 0), pixmap.size());
    const QMargins sourceMargins(qMin(m_sourceMargins.left(), sourceRect.width() / 2 - 1),
                                 qMin(m_sourceMargins.top(), sourceRect.height() / 2 - 1),
                                 qMin(m_sourceMargins.right(), sourceRect.width() / 2 - 1),
                                 qMin(m_sourceMargins.bottom(), sourceRect.height() / 2 - 1));
    const QMargins targetMargins = scaledTargetMargins(sourceRect.size(), targetRect.size(), sourceMargins);

    const int sx0 = sourceRect.left();
    const int sx1 = sourceRect.left() + sourceMargins.left();
    const int sx2 = sourceRect.right() - sourceMargins.right() + 1;
    const int sx3 = sourceRect.right() + 1;
    const int sy0 = sourceRect.top();
    const int sy1 = sourceRect.top() + sourceMargins.top();
    const int sy2 = sourceRect.bottom() - sourceMargins.bottom() + 1;
    const int sy3 = sourceRect.bottom() + 1;

    const int dx0 = targetRect.left();
    const int dx1 = targetRect.left() + targetMargins.left();
    const int dx2 = targetRect.right() - targetMargins.right() + 1;
    const int dx3 = targetRect.right() + 1;
    const int dy0 = targetRect.top();
    const int dy1 = targetRect.top() + targetMargins.top();
    const int dy2 = targetRect.bottom() - targetMargins.bottom() + 1;
    const int dy3 = targetRect.bottom() + 1;

    const int sourceXs[] = {sx0, sx1, sx2, sx3};
    const int sourceYs[] = {sy0, sy1, sy2, sy3};
    const int targetXs[] = {dx0, dx1, dx2, dx3};
    const int targetYs[] = {dy0, dy1, dy2, dy3};

    for (int row = 0; row < 3; ++row) {
        for (int column = 0; column < 3; ++column) {
            const QRect sourcePart(QPoint(sourceXs[column], sourceYs[row]),
                                   QPoint(sourceXs[column + 1] - 1, sourceYs[row + 1] - 1));
            const QRect targetPart(QPoint(targetXs[column], targetYs[row]),
                                   QPoint(targetXs[column + 1] - 1, targetYs[row + 1] - 1));
            if (!sourcePart.isEmpty() && !targetPart.isEmpty()) {
                painter.drawPixmap(targetPart, pixmap, sourcePart);
            }
        }
    }
}
