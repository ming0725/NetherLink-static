#include "GlobalNotification.h"

#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"

#include <QApplication>
#include <QEvent>
#include <QFont>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGraphicsOpacityEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QParallelAnimationGroup>
#include <QPointer>
#include <QPropertyAnimation>
#include <QTimer>

#include <algorithm>

namespace {

const QString kBackgroundSource(QStringLiteral(":/resources/icon/global_notification.png"));
const QString kSuccessIconSource(QStringLiteral(":/resources/icon/correct.png"));
const QString kFailureIconSource(QStringLiteral(":/resources/icon/fail.png"));
const QString kMinecraftFontSource(QStringLiteral(":/resources/font/MinecraftAE.ttf"));

// Notification layout tuning constants.
constexpr int kNotificationWidth = 260;
constexpr int kNotificationHeight = 60;
constexpr int kRightMargin = 2;
constexpr int kTopMargin = 32;
constexpr int kMaxStackedNotifications = 4;
constexpr int kStackRightOffsets[kMaxStackedNotifications] = {0, 12, 22, 30};
constexpr int kStackDownOffsets[kMaxStackedNotifications] = {0, 10, 18, 24};
constexpr qreal kStackOpacities[kMaxStackedNotifications] = {1.0, 0.72, 0.46, 0.24};
constexpr int kExpandedStackGap = 8;
constexpr int kCloseButtonSize = 14;
constexpr int kCloseButtonMargin = 1;
constexpr int kCloseLineHalfLength = 2;
constexpr int kIconMaxSize = 32;
constexpr int kIconLeft = 16;
constexpr int kTextLeft = 62;
constexpr int kFirstLineTop = 12;
constexpr int kSecondLineTop = 32;
constexpr int kLineHeight = 17;
constexpr int kTextRightPadding = 16;
constexpr int kTextFontSize = 13;
constexpr int kSlideDuration = 260;
constexpr int kHoldDuration = 1900;
constexpr int kDismissDuration = 220;

QPointer<QWidget> expandedParent;

quint64 nextNotificationSequence()
{
    static quint64 sequence = 0;
    return ++sequence;
}

QWidget* notificationParentFor(QWidget* attachedWidget)
{
    QWidget* parent = attachedWidget ? attachedWidget->window() : QApplication::activeWindow();
    return parent ? parent : attachedWidget;
}

QPoint mouseGlobalPosition(QMouseEvent* event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}

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

QFont notificationFont(const QFont& fallback)
{
    static const QString family = [] {
        const int fontId = QFontDatabase::addApplicationFont(kMinecraftFontSource);
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
    font.setPixelSize(kTextFontSize);
    font.setBold(true);
    return font;
}

} // namespace

GlobalNotification::GlobalNotification(QWidget* attachedWidget,
                                       Type type,
                                       const QString& message,
                                       QWidget* parent)
    : QWidget(parent ? parent : notificationParentFor(attachedWidget))
    , m_type(type)
    , m_message(message)
    , m_attachedWidget(parent ? parent : notificationParentFor(attachedWidget))
    , m_sourceMargins(36, 36, 36, 36)
    , m_sequence(nextNotificationSequence())
{
    setMinimumSize(kNotificationWidth - kStackRightOffsets[kMaxStackedNotifications - 1], kNotificationHeight);
    setMaximumSize(kNotificationWidth, kNotificationHeight);
    resize(kNotificationWidth, kNotificationHeight);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    setMouseTracking(true);
    setFocusPolicy(Qt::NoFocus);

    m_opacityEffect = new QGraphicsOpacityEffect(this);
    m_opacityEffect->setOpacity(1.0);
    setGraphicsEffect(m_opacityEffect);

    m_dismissTimer = new QTimer(this);
    m_dismissTimer->setSingleShot(true);
    connect(m_dismissTimer, &QTimer::timeout, this, [this] {
        m_dismissTimerRunning = false;
        m_remainingDismissMs = 0;
        dismiss(true);
    });

    if (m_attachedWidget) {
        m_attachedWidget->installEventFilter(this);
    }
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->installEventFilter(this);
    }
}

GlobalNotification::~GlobalNotification()
{
    if (m_attachedWidget) {
        m_attachedWidget->removeEventFilter(this);
    }
    if (QCoreApplication::instance()) {
        QCoreApplication::instance()->removeEventFilter(this);
    }
}

void GlobalNotification::showSuccess(QWidget* attachedWidget, const QString& message)
{
    showNotification(attachedWidget, Type::Success, message);
}

void GlobalNotification::showFailure(QWidget* attachedWidget, const QString& message)
{
    showNotification(attachedWidget, Type::Failure, message);
}

void GlobalNotification::showNotification(QWidget* attachedWidget, Type type, const QString& message)
{
    QWidget* parent = notificationParentFor(attachedWidget);
    if (!parent) {
        return;
    }

    auto* notification = new GlobalNotification(parent, type, message, parent);
    notification->start();
    AudioService::instance().play(AudioService::SoundEffect::SuccessfulHit);
}

bool GlobalNotification::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_attachedWidget
        && (event->type() == QEvent::Resize || event->type() == QEvent::Show)) {
        updateForParentResize();
    }

    if (event->type() == QEvent::MouseButtonPress && isExpanded(parentWidget())) {
        auto* mouseEvent = static_cast<QMouseEvent*>(event);
        if (!containsGlobalPos(parentWidget(), mouseGlobalPosition(mouseEvent))) {
            collapseNotifications(parentWidget());
        }
    }

    return QWidget::eventFilter(watched, event);
}

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
void GlobalNotification::enterEvent(QEnterEvent* event)
#else
void GlobalNotification::enterEvent(QEvent* event)
#endif
{
    m_hovered = true;
    update();
    pauseDismissTimers(parentWidget());
    QWidget::enterEvent(event);
}

void GlobalNotification::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    if (!isExpanded(parentWidget()) && !containsCursor(parentWidget())) {
        resumeDismissTimers(parentWidget());
    }
    QWidget::leaveEvent(event);
}

void GlobalNotification::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (closeButtonRect().contains(event->pos())) {
            clearNotifications(parentWidget());
            event->accept();
            return;
        }

        expandNotifications(parentWidget());
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void GlobalNotification::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    const QPixmap background = ImageService::instance().pixmap(kBackgroundSource);
    if (background.isNull()) {
        painter.fillRect(rect(), QColor(45, 45, 45, 235));
    } else {
        drawNinePatch(painter, background, rect());
    }

    const QString iconSource = m_type == Type::Success ? kSuccessIconSource : kFailureIconSource;
    const QRect iconBounds(kIconLeft, (height() - kIconMaxSize) / 2, kIconMaxSize, kIconMaxSize);
    const QPixmap icon = ImageService::instance().pixmap(iconSource);
    if (!icon.isNull()) {
        QSize iconSize = icon.size();
        iconSize.scale(iconBounds.size(), Qt::KeepAspectRatio);
        const QRect iconRect(QPoint(iconBounds.x() + (iconBounds.width() - iconSize.width()) / 2,
                                    iconBounds.y() + (iconBounds.height() - iconSize.height()) / 2),
                             iconSize);
        painter.drawPixmap(iconRect, icon);
    }

    const QFont textFont = notificationFont(font());
    painter.setFont(textFont);
    const QFontMetrics metrics(textFont);
    const QRect firstLineRect(kTextLeft,
                              kFirstLineTop,
                              width() - kTextLeft - kTextRightPadding,
                              kLineHeight);
    const QRect secondLineRect(kTextLeft,
                               kSecondLineTop,
                               width() - kTextLeft - kTextRightPadding,
                               kLineHeight);
    const QString title = m_type == Type::Success
            ? QStringLiteral("操作已达成")
            : QStringLiteral("操作未达成");
    const QString detail = metrics.elidedText(m_message, Qt::ElideRight, secondLineRect.width());

    painter.setPen(QColor(0, 0, 0, 185));
    painter.drawText(firstLineRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.drawText(secondLineRect.translated(1, 1), Qt::AlignLeft | Qt::AlignVCenter, detail);

    painter.setPen(QColor(0xFF, 0xFF, 0x55));
    painter.drawText(firstLineRect, Qt::AlignLeft | Qt::AlignVCenter, title);
    painter.setPen(QColor(0xFF, 0xFF, 0xFF));
    painter.drawText(secondLineRect, Qt::AlignLeft | Qt::AlignVCenter, detail);

    if (m_hovered) {
        const QRectF closeRect(closeButtonRect());
        const QPointF center = closeRect.center();
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(255, 255, 255, 220));
        painter.drawEllipse(closeRect);

        QPen closePen(QColor(0, 0, 0, 210), 2);
        closePen.setCapStyle(Qt::RoundCap);
        painter.setPen(closePen);
        painter.drawLine(QPointF(center.x() - kCloseLineHalfLength,
                                 center.y() - kCloseLineHalfLength),
                         QPointF(center.x() + kCloseLineHalfLength,
                                 center.y() + kCloseLineHalfLength));
        painter.drawLine(QPointF(center.x() + kCloseLineHalfLength,
                                 center.y() - kCloseLineHalfLength),
                         QPointF(center.x() - kCloseLineHalfLength,
                                 center.y() + kCloseLineHalfLength));
    }
}

void GlobalNotification::start()
{
    setGeometry(hiddenGeometry(0));
    show();

    relayoutNotifications(parentWidget(), true);
    restartDismissTimer();
    if (isExpanded(parentWidget()) || containsCursor(parentWidget())) {
        pauseDismissTimer();
    }
}

QRect GlobalNotification::closeButtonRect() const
{
    return {kCloseButtonMargin,
            kCloseButtonMargin,
            kCloseButtonSize,
            kCloseButtonSize};
}

QRect GlobalNotification::stackGeometry(int depth) const
{
    if (!m_attachedWidget) {
        return {};
    }

    const int stackDepth = qBound(0, depth, kMaxStackedNotifications - 1);
    const int rightOffset = kStackRightOffsets[stackDepth];
    const int x = qMax(0, m_attachedWidget->width() - kNotificationWidth - kRightMargin) + rightOffset;
    const int y = kTopMargin + kStackDownOffsets[stackDepth];
    const int width = kNotificationWidth - rightOffset;
    return {x, y, width, kNotificationHeight};
}

QRect GlobalNotification::expandedGeometry(int depth) const
{
    if (!m_attachedWidget) {
        return {};
    }

    const int x = qMax(0, m_attachedWidget->width() - kNotificationWidth - kRightMargin);
    const int y = kTopMargin + depth * (kNotificationHeight + kExpandedStackGap);
    return {x, y, kNotificationWidth, kNotificationHeight};
}

QRect GlobalNotification::hiddenGeometry(int depth) const
{
    if (!m_attachedWidget) {
        return {};
    }

    QRect geometry = stackGeometry(depth);
    geometry.moveLeft(m_attachedWidget->width() + kRightMargin);
    return geometry;
}

qreal GlobalNotification::stackOpacity(int depth) const
{
    const int stackDepth = qBound(0, depth, kMaxStackedNotifications - 1);
    return kStackOpacities[stackDepth];
}

QParallelAnimationGroup* GlobalNotification::animateTo(const QRect& targetGeometry,
                                                       qreal targetOpacity,
                                                       int duration,
                                                       QEasingCurve::Type easingCurve)
{
    stopLayoutAnimation();

    if (duration <= 0) {
        setGeometry(targetGeometry);
        if (m_opacityEffect) {
            m_opacityEffect->setOpacity(targetOpacity);
        }
        return nullptr;
    }

    auto* group = new QParallelAnimationGroup(this);
    m_layoutAnimation = group;

    auto* geometryAnimation = new QPropertyAnimation(this, "geometry", group);
    geometryAnimation->setDuration(duration);
    geometryAnimation->setEasingCurve(easingCurve);
    geometryAnimation->setStartValue(geometry());
    geometryAnimation->setEndValue(targetGeometry);
    group->addAnimation(geometryAnimation);

    if (m_opacityEffect) {
        auto* opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", group);
        opacityAnimation->setDuration(duration);
        opacityAnimation->setEasingCurve(easingCurve);
        opacityAnimation->setStartValue(m_opacityEffect->opacity());
        opacityAnimation->setEndValue(targetOpacity);
        group->addAnimation(opacityAnimation);
    }

    connect(group, &QParallelAnimationGroup::finished, this, [this, group] {
        if (m_layoutAnimation == group) {
            m_layoutAnimation = nullptr;
        }
        group->deleteLater();
    });
    group->start();
    return group;
}

void GlobalNotification::stopLayoutAnimation()
{
    if (!m_layoutAnimation) {
        return;
    }

    QParallelAnimationGroup* animation = m_layoutAnimation;
    m_layoutAnimation = nullptr;
    animation->stop();
    animation->deleteLater();
}

void GlobalNotification::animateToStackDepth(int depth, bool animated)
{
    if (m_dismissing) {
        return;
    }

    const int duration = animated ? kSlideDuration : 0;
    const QRect targetGeometry = isExpanded(parentWidget()) ? expandedGeometry(depth) : stackGeometry(depth);
    const qreal targetOpacity = isExpanded(parentWidget()) ? 1.0 : stackOpacity(depth);
    animateTo(targetGeometry, targetOpacity, duration, QEasingCurve::OutCubic);
}

void GlobalNotification::dismiss(bool slideToHidden)
{
    if (m_dismissing) {
        return;
    }

    m_dismissing = true;
    pauseDismissTimer();
    QRect targetGeometry = geometry();
    if (slideToHidden && m_attachedWidget) {
        targetGeometry.moveLeft(m_attachedWidget->width() + kRightMargin);
    }
    QParallelAnimationGroup* animation = animateTo(targetGeometry, 0.0, kDismissDuration, QEasingCurve::InCubic);
    if (!animation) {
        hide();
        QWidget* parent = parentWidget();
        deleteLater();
        relayoutNotifications(parent, true);
        resumeDismissTimers(parent);
        return;
    }

    connect(animation, &QParallelAnimationGroup::finished, this, [this] {
        QWidget* parent = parentWidget();
        hide();
        deleteLater();
        relayoutNotifications(parent, true);
        resumeDismissTimers(parent);
    });
}

void GlobalNotification::startDismissTimer(int duration)
{
    if (m_dismissing || duration <= 0) {
        return;
    }

    m_remainingDismissMs = duration;
    if (isExpanded(parentWidget()) || containsCursor(parentWidget())) {
        return;
    }

    m_dismissElapsed.restart();
    m_dismissTimerRunning = true;
    m_dismissTimer->start(duration);
}

void GlobalNotification::pauseDismissTimer()
{
    if (!m_dismissTimerRunning) {
        return;
    }

    m_remainingDismissMs = qMax(0, m_remainingDismissMs - static_cast<int>(m_dismissElapsed.elapsed()));
    m_dismissTimer->stop();
    m_dismissTimerRunning = false;
}

void GlobalNotification::resumeDismissTimer()
{
    if (m_dismissing || m_dismissTimerRunning || m_remainingDismissMs <= 0
        || isExpanded(parentWidget()) || containsCursor(parentWidget())) {
        return;
    }

    m_dismissElapsed.restart();
    m_dismissTimerRunning = true;
    m_dismissTimer->start(m_remainingDismissMs);
}

void GlobalNotification::restartDismissTimer()
{
    if (m_dismissing) {
        return;
    }

    if (m_dismissTimer) {
        m_dismissTimer->stop();
    }
    m_dismissTimerRunning = false;
    startDismissTimer(kHoldDuration);
}

void GlobalNotification::updateForParentResize()
{
    if (!isVisible()) {
        return;
    }

    relayoutNotifications(parentWidget(), false);
}

QVector<GlobalNotification*> GlobalNotification::activeNotifications(QWidget* parent)
{
    if (!parent) {
        return {};
    }

    QVector<GlobalNotification*> notifications;
    const auto children = parent->findChildren<GlobalNotification*>(QString(), Qt::FindDirectChildrenOnly);
    notifications.reserve(children.size());
    for (GlobalNotification* notification : children) {
        if (notification->isVisible() && !notification->m_dismissing) {
            notifications.push_back(notification);
        }
    }

    std::sort(notifications.begin(), notifications.end(), [](const GlobalNotification* lhs,
                                                             const GlobalNotification* rhs) {
        return lhs->m_sequence > rhs->m_sequence;
    });
    return notifications;
}

void GlobalNotification::relayoutNotifications(QWidget* parent, bool animated)
{
    QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (int i = notifications.size() - 1; i >= 0; --i) {
        notifications[i]->raise();
    }

    for (int i = 0; i < notifications.size(); ++i) {
        GlobalNotification* notification = notifications[i];
        if (i < kMaxStackedNotifications) {
            notification->animateToStackDepth(i, animated);
        } else {
            notification->dismiss(false);
        }
    }
}

bool GlobalNotification::isExpanded(QWidget* parent)
{
    return parent && expandedParent == parent;
}

bool GlobalNotification::containsGlobalPos(QWidget* parent, const QPoint& globalPos)
{
    const QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (GlobalNotification* notification : notifications) {
        if (notification->rect().contains(notification->mapFromGlobal(globalPos))) {
            return true;
        }
    }
    return false;
}

bool GlobalNotification::containsCursor(QWidget* parent)
{
    return containsGlobalPos(parent, QCursor::pos());
}

void GlobalNotification::expandNotifications(QWidget* parent)
{
    if (!parent || isExpanded(parent)) {
        return;
    }

    if (expandedParent && expandedParent != parent) {
        collapseNotifications(expandedParent);
    }

    expandedParent = parent;
    pauseDismissTimers(parent);
    relayoutNotifications(parent, true);
}

void GlobalNotification::collapseNotifications(QWidget* parent)
{
    if (!parent || !isExpanded(parent)) {
        return;
    }

    expandedParent = nullptr;
    relayoutNotifications(parent, true);
    restartDismissTimers(parent);
}

void GlobalNotification::clearNotifications(QWidget* parent)
{
    if (!parent) {
        return;
    }

    if (isExpanded(parent)) {
        expandedParent = nullptr;
    }

    const QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (GlobalNotification* notification : notifications) {
        notification->dismiss(true);
    }
}

void GlobalNotification::pauseDismissTimers(QWidget* parent)
{
    const QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (GlobalNotification* notification : notifications) {
        notification->pauseDismissTimer();
    }
}

void GlobalNotification::resumeDismissTimers(QWidget* parent)
{
    if (isExpanded(parent) || containsCursor(parent)) {
        return;
    }

    const QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (GlobalNotification* notification : notifications) {
        notification->resumeDismissTimer();
    }
}

void GlobalNotification::restartDismissTimers(QWidget* parent)
{
    if (isExpanded(parent) || containsCursor(parent)) {
        return;
    }

    const QVector<GlobalNotification*> notifications = activeNotifications(parent);
    for (GlobalNotification* notification : notifications) {
        notification->restartDismissTimer();
    }
}

void GlobalNotification::drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const
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
