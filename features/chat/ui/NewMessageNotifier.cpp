#include "NewMessageNotifier.h"

#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

#include <QEvent>
#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

namespace {

constexpr int kHeight = 34;
constexpr int kMinWidth = 62;
constexpr int kHorizontalPadding = 10;
constexpr int kIconSize = 20;
constexpr int kIconTextGap = 6;
constexpr int kCornerRadius = 15;
const QString kIconSource = QStringLiteral(":/resources/icon/ender_pearl.png");

} // namespace

NewMessageNotifier::NewMessageNotifier(QWidget *parent) : QWidget(parent)
{
    setFixedHeight(kHeight);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(10);
    shadow->setOffset(0, 2);
    shadow->setColor(QColor(0, 0, 0, 54));
    setGraphicsEffect(shadow);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        update();
    });

    QWidget::hide();
}

void NewMessageNotifier::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    QPainterPath path;
    const QRectF pillRect = rect().adjusted(1, 1, -1, -1);
    path.addRoundedRect(pillRect, kCornerRadius, kCornerRadius);

    painter.setPen(Qt::NoPen);
    QColor background;
    QColor foreground;
    if (ThemeManager::instance().isDark()) {
        background = m_pressed ? QColor(0x22, 0x24, 0x29)
                               : (m_hovered ? QColor(0x1b, 0x1d, 0x22) : QColor(0x0f, 0x10, 0x13));
        foreground = Qt::white;
    } else {
        background = m_pressed ? QColor(0xee, 0xee, 0xee)
                               : (m_hovered ? QColor(0xf8, 0xf8, 0xf8) : Qt::white);
        foreground = QColor(0x12, 0x12, 0x12);
    }
    painter.setBrush(background);
    painter.drawPath(path);

    const QRect contentRect = rect().adjusted(kHorizontalPadding, 0, -kHorizontalPadding, 0);
    const QRect iconRect(contentRect.left(),
                         (height() - kIconSize) / 2,
                         kIconSize,
                         kIconSize);
    const QPixmap icon = ImageService::instance().scaled(kIconSource,
                                                         iconRect.size(),
                                                         Qt::KeepAspectRatio,
                                                         painter.device()->devicePixelRatioF());
    if (!icon.isNull()) {
        painter.drawPixmap(iconRect, icon);
    }

    QFont countFont = font();
    countFont.setPixelSize(14);
    countFont.setWeight(QFont::DemiBold);
    painter.setFont(countFont);
    painter.setPen(foreground);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    const QRect textRect(iconRect.right() + 1 + kIconTextGap,
                         0,
                         qMax(0, contentRect.right() - iconRect.right() - kIconTextGap),
                         height());
    painter.drawText(textRect, Qt::AlignVCenter | Qt::AlignRight, displayText());
}

void NewMessageNotifier::mousePressEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    m_pressed = true;
    update();
    playClickSound();
    event->accept();
}

void NewMessageNotifier::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }

    const bool wasPressed = m_pressed;
    m_pressed = false;
    update();
    if (wasPressed && rect().contains(event->pos())) {
        emit clicked();
    }
    event->accept();
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void NewMessageNotifier::enterEvent(QEnterEvent *event)
#else
void NewMessageNotifier::enterEvent(QEvent *event)
#endif
{
    QWidget::enterEvent(event);
    m_hovered = true;
    update();
}

void NewMessageNotifier::leaveEvent(QEvent *event)
{
    QWidget::leaveEvent(event);
    m_hovered = false;
    m_pressed = false;
    update();
}

void NewMessageNotifier::setMessageCount(int count)
{
    m_count = qMax(0, count);
    updateSize();
    update();
}

QSize NewMessageNotifier::sizeHint() const
{
    QFont countFont = font();
    countFont.setPixelSize(14);
    countFont.setWeight(QFont::DemiBold);
    const QFontMetrics metrics(countFont);
    const int textWidth = metrics.horizontalAdvance(displayText());
    const int width = qMax(kMinWidth,
                           kHorizontalPadding * 2 + kIconSize + kIconTextGap + textWidth);
    return QSize(width, kHeight);
}

QString NewMessageNotifier::displayText() const
{
    if (m_count >= 100) {
        return QStringLiteral("99+");
    }
    return QString::number(qMax(1, m_count));
}

void NewMessageNotifier::updateSize()
{
    setFixedSize(sizeHint());
}

void NewMessageNotifier::playClickSound()
{
    AudioService::instance().playPortalClick();
}
