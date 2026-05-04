#include "MinecraftButton.h"

#include "shared/services/ImageService.h"

#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QPainter>
#include <QPaintEvent>

namespace {

const QString kDefaultNormalImage(QStringLiteral(":/resources/icon/mc_button.png"));
const QString kDefaultHoverImage(QStringLiteral(":/resources/icon/mc_button_highlighted.png"));
constexpr int kDefaultButtonHeight = 40;
constexpr int kMinimumButtonWidth = 72;

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

MinecraftButton::MinecraftButton(QWidget* parent)
    : QPushButton(parent)
    , m_normalImage(kDefaultNormalImage)
    , m_hoverImage(kDefaultHoverImage)
    , m_sourceMargins(5, 5, 5, 5)
{
    initialize();
}

MinecraftButton::MinecraftButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
    , m_normalImage(kDefaultNormalImage)
    , m_hoverImage(kDefaultHoverImage)
    , m_sourceMargins(5, 5, 5, 5)
{
    initialize();
}

void MinecraftButton::initialize()
{
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::NoFocus);
    setMinimumSize(minimumSizeHint());

    QFont buttonFont = font();
    buttonFont.setBold(true);
    setFont(buttonFont);
}

void MinecraftButton::setNormalImage(const QString& source)
{
    if (m_normalImage == source) {
        return;
    }

    m_normalImage = source;
    update();
}

void MinecraftButton::setHoverImage(const QString& source)
{
    if (m_hoverImage == source) {
        return;
    }

    m_hoverImage = source;
    update();
}

void MinecraftButton::setDisabledImage(const QString& source)
{
    if (m_disabledImage == source) {
        return;
    }

    m_disabledImage = source;
    update();
}

void MinecraftButton::setSourceMargins(const QMargins& margins)
{
    if (m_sourceMargins == margins) {
        return;
    }

    m_sourceMargins = margins;
    update();
}

QSize MinecraftButton::sizeHint() const
{
    const int textWidth = QFontMetrics(font()).horizontalAdvance(text());
    return {qMax(kMinimumButtonWidth, textWidth + 42), kDefaultButtonHeight};
}

QSize MinecraftButton::minimumSizeHint() const
{
    const int textWidth = QFontMetrics(font()).horizontalAdvance(text());
    return {qMax(kMinimumButtonWidth, textWidth + 28), kDefaultButtonHeight};
}

bool MinecraftButton::event(QEvent* event)
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
        setHovered(false);
        break;
    case QEvent::EnabledChange:
        setHovered(false);
        break;
    default:
        break;
    }

    return QPushButton::event(event);
}

void MinecraftButton::setHovered(bool hovered)
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

QString MinecraftButton::currentImage() const
{
    if (!isEnabled() && !m_disabledImage.isEmpty()) {
        return m_disabledImage;
    }
    return (isEnabled() && m_hovered) ? m_hoverImage : m_normalImage;
}

void MinecraftButton::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QPixmap pixmap = ImageService::instance().pixmap(currentImage());
    if (pixmap.isNull()) {
        painter.fillRect(rect(), isEnabled() ? QColor(0x75, 0x75, 0x75) : QColor(0x55, 0x55, 0x55));
    } else {
        drawNinePatch(painter, pixmap, rect());
    }

    if (!isEnabled() && m_disabledImage.isEmpty()) {
        painter.fillRect(rect(), QColor(0, 0, 0, 90));
    }

    const QRect textRect = rect().adjusted(10, 0, -10, 0);
    const QColor shadowColor(0, 0, 0, isEnabled() ? 180 : 110);
    const QColor textColor = isEnabled() ? QColor(0xFF, 0xFF, 0xFF) : QColor(0xB0, 0xB0, 0xB0);

    painter.setPen(shadowColor);
    painter.drawText(textRect.translated(1, 1), Qt::AlignCenter, text());
    painter.setPen(textColor);
    painter.drawText(textRect, Qt::AlignCenter, text());
}

void MinecraftButton::drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const
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
