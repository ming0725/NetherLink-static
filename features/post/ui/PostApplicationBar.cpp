#include "PostApplicationBar.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/QtFallbackLiquidGlass.h"

#include <QFontMetrics>
#include <QGraphicsDropShadowEffect>
#include <QMouseEvent>
#include <QPainter>
#include <QtGlobal>

#ifdef Q_OS_MACOS
#include "platform/macos/MacPostBarBridge_p.h"
#endif

namespace {

constexpr auto kSystemFloatingBarsSuppressedProperty = "systemFloatingBarsSuppressed";
constexpr qreal kPostBarCornerRadius = 15.0;

} // namespace

PostApplicationBar::PostApplicationBar(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);
    m_liquidGlass = new QtFallbackLiquidGlassController(this);
    m_liquidGlass->setEffectMode(QtFallbackLiquidGlassController::EffectMode::GaussianBlur);
    m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                            kPostBarCornerRadius);

#ifdef Q_OS_MACOS
    m_usesNativeBar = MacPostBarBridge::appearance() != MacPostBarBridge::Appearance::Unsupported;
#endif

    updatePanelShadow();

    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(300);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged,
            this, &PostApplicationBar::onHighlightValueChanged);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        syncPlatformBar();
        updatePanelShadow();
        updateQtFallbackLiquidGlassState();
        update();
    });

    initItems();
    resize(sizeHint());
    layoutItems();
}

PostApplicationBar::~PostApplicationBar()
{
    releaseQtFallbackLiquidGlassResources(false);
#ifdef Q_OS_MACOS
    if (m_usesNativeBar) {
        MacPostBarBridge::clearBar(this);
    }
#endif
}

bool PostApplicationBar::event(QEvent* event)
{
    const bool handled = QWidget::event(event);

#ifdef Q_OS_MACOS
    if (m_usesNativeBar) {
        switch (event->type()) {
        case QEvent::Show:
        case QEvent::Move:
        case QEvent::Resize:
        case QEvent::WinIdChange:
        case QEvent::ParentChange:
        case QEvent::ZOrderChange:
            syncPlatformBar();
            break;
        case QEvent::Hide:
            MacPostBarBridge::clearBar(this);
            break;
        default:
            break;
        }
        return handled;
    }
#endif

    switch (event->type()) {
    case QEvent::Show:
        updateQtFallbackLiquidGlassState();
        break;
    case QEvent::Move:
    case QEvent::Resize:
    case QEvent::ParentChange:
    case QEvent::ZOrderChange:
        scheduleLiquidGlassUpdate(QtFallbackLiquidGlassController::refreshDelayMs());
        break;
    case QEvent::Hide:
        releaseQtFallbackLiquidGlassResources();
        break;
    default:
        break;
    }
    if (m_liquidGlass) {
        m_liquidGlass->handleHostEvent(event);
    }

    return handled;
}

void PostApplicationBar::initItems()
{
    static const QStringList labels = {
        QStringLiteral("首页"),
        QStringLiteral("关注"),
        QStringLiteral("发表"),
        QStringLiteral("消息"),
        QStringLiteral("我")
    };

    const QFontMetrics metrics(font());
    items.clear();
    items.reserve(labels.size());
    for (const QString& label : labels) {
        TabItem item;
        item.label = label;
        item.widthHint = qMax(minItemWidth, metrics.horizontalAdvance(label) + 28);
        items.push_back(item);
    }

    selectedIndex = items.isEmpty() ? -1 : 0;
    hoveredIndex = -1;
}

void PostApplicationBar::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    layoutItems();
    if (m_liquidGlass) {
        m_liquidGlass->setShape(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5),
                                kPostBarCornerRadius);
    }
    scheduleLiquidGlassUpdate(0);
}

QSize PostApplicationBar::minimumSizeHint() const
{
    return sizeHint();
}

void PostApplicationBar::setVisualOpacity(qreal opacity)
{
    const qreal bounded = qBound<qreal>(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_visualOpacity, bounded)) {
        return;
    }

    m_visualOpacity = bounded;
    update();
    syncPlatformBar();
}

void PostApplicationBar::setLiquidGlassSourceWidget(QWidget* widget)
{
    if (!m_liquidGlass) {
        return;
    }
    m_liquidGlass->setSourceWidget(widget);
    updateQtFallbackLiquidGlassState();
}

void PostApplicationBar::scheduleLiquidGlassUpdate(int delayMs)
{
    if (!m_liquidGlass || !shouldUseQtFallbackLiquidGlass()) {
        return;
    }
    m_liquidGlass->scheduleUpdate(delayMs);
}

void PostApplicationBar::scheduleLiquidGlassInteractiveUpdate()
{
    if (!m_liquidGlass || !shouldUseQtFallbackLiquidGlass()) {
        return;
    }
    m_liquidGlass->scheduleInteractiveUpdate();
}

bool PostApplicationBar::usesQtFallbackLiquidGlass() const
{
    return shouldUseQtFallbackLiquidGlass();
}

void PostApplicationBar::refreshPlatformAppearance()
{
#ifdef Q_OS_MACOS
    const bool shouldUseNative = MacPostBarBridge::appearance()
            != MacPostBarBridge::Appearance::Unsupported;
    const bool systemSuppressed = property(kSystemFloatingBarsSuppressedProperty).toBool();

    if (m_usesNativeBar && !shouldUseNative) {
        MacPostBarBridge::clearBar(this);
        m_usesNativeBar = false;
        updatePanelShadow();
        layoutItems();
        updateQtFallbackLiquidGlassState();
        update();
        return;
    }

    if (!m_usesNativeBar && shouldUseNative && systemSuppressed) {
        layoutItems();
        updateQtFallbackLiquidGlassState();
        update();
        return;
    }

    if (!m_usesNativeBar && shouldUseNative) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        releaseQtFallbackLiquidGlassResources(false);
        m_usesNativeBar = true;
        syncPlatformBar();
        update();
        return;
    }

    if (m_usesNativeBar) {
        syncPlatformBar();
    }
#endif

    updatePanelShadow();
    updateQtFallbackLiquidGlassState();
    update();
}

QSize PostApplicationBar::sizeHint() const
{
    int totalWidth = margin * 2;
    for (int i = 0; i < items.size(); ++i) {
        totalWidth += items.at(i).widthHint;
        if (i + 1 < items.size()) {
            totalWidth += spacing;
        }
    }
    return QSize(totalWidth, itemHeight + margin * 2);
}

void PostApplicationBar::layoutItems()
{
    int startX = margin;
    const int y = height() - itemHeight - margin;
    for (TabItem& item : items) {
        item.rect = QRect(startX, y, item.widthHint, itemHeight);
        startX += item.widthHint + spacing;
    }

    if (selectedIndex >= 0 && selectedIndex < items.size()) {
        highlightX = items.at(selectedIndex).rect.x();
    } else {
        highlightX = 0;
    }
    updateSelectedRect();
}

void PostApplicationBar::updateSelectedRect()
{
    if (selectedIndex < 0 || selectedIndex >= items.size()) {
        selectedRect = {};
        return;
    }

    selectedRect = QRect(highlightX,
                         height() - itemHeight - margin,
                         items.at(selectedIndex).widthHint,
                         itemHeight);
}

int PostApplicationBar::indexAtPosition(const QPoint& pos) const
{
    for (int index = 0; index < items.size(); ++index) {
        if (items.at(index).rect.contains(pos)) {
            return index;
        }
    }
    return -1;
}

void PostApplicationBar::paintEvent(QPaintEvent*)
{
    if (m_usesNativeBar) {
        return;
    }

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.setOpacity(m_visualOpacity);
    painter.setPen(Qt::NoPen);

    const QRectF contentRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal outerRadius = kPostBarCornerRadius;

    const bool usesGaussianFallback = shouldUseQtFallbackLiquidGlass() && m_liquidGlass;
    if (usesGaussianFallback) {
        m_liquidGlass->setShape(contentRect, outerRadius);
        m_liquidGlass->paint(painter);
    } else {
        QColor barBackground = ThemeManager::instance().color(ThemeColor::PanelBackground);
        barBackground.setAlpha(ThemeManager::instance().isDark() ? 190 : 205);
        painter.setBrush(barBackground);
        painter.drawRoundedRect(contentRect, outerRadius, outerRadius);
    }

    if (!selectedRect.isEmpty()) {
        QColor selectedBackground = ThemeManager::instance().color(ThemeColor::PostBarItemSelectedBackground);
        if (usesGaussianFallback) {
            selectedBackground.setAlpha(ThemeManager::instance().isDark() ? 230 : 40);
        }
        painter.setBrush(selectedBackground);
        painter.drawRoundedRect(selectedRect, 10, 10);
    }

    painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
    for (const TabItem& item : items) {
        painter.drawText(item.rect, Qt::AlignCenter, item.label);
    }

    if (!shouldUseQtFallbackLiquidGlass()) {
        const qreal borderWidth = 2.0;
        const qreal inset = borderWidth / 2.0;
        const QRectF borderRect = rect().adjusted(inset, inset, -inset, -inset);
        QPen pen(ThemeManager::instance().color(ThemeColor::Accent));
        pen.setWidthF(borderWidth);
        pen.setJoinStyle(Qt::RoundJoin);
        pen.setCapStyle(Qt::RoundCap);
        painter.setPen(pen);
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(borderRect, outerRadius - inset, outerRadius - inset);
    }
}

void PostApplicationBar::mouseMoveEvent(QMouseEvent* event)
{
    if (m_usesNativeBar) {
        QWidget::mouseMoveEvent(event);
        return;
    }

    const int index = indexAtPosition(event->pos());
    if (hoveredIndex != index) {
        hoveredIndex = index;
        if (hoveredIndex >= 0) {
            setCursor(Qt::PointingHandCursor);
        } else {
            unsetCursor();
        }
    }
    QWidget::mouseMoveEvent(event);
}

void PostApplicationBar::leaveEvent(QEvent* event)
{
    if (m_usesNativeBar) {
        QWidget::leaveEvent(event);
        return;
    }

    hoveredIndex = -1;
    unsetCursor();
    QWidget::leaveEvent(event);
}

void PostApplicationBar::mousePressEvent(QMouseEvent* event)
{
    if (m_usesNativeBar) {
        QWidget::mousePressEvent(event);
        return;
    }

    if (event->button() != Qt::LeftButton) {
        QWidget::mousePressEvent(event);
        return;
    }

    const int index = indexAtPosition(event->pos());
    if (index >= 0) {
        onItemClicked(index);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void PostApplicationBar::onItemClicked(int index)
{
    if (index < 0 || index >= items.size() || selectedIndex == index) {
        return;
    }

    const int startX = highlightX;
    const int endX = items.at(index).rect.x();
    selectedIndex = index;

    if (m_usesNativeBar) {
        highlightX = endX;
        updateSelectedRect();
#ifdef Q_OS_MACOS
        MacPostBarBridge::syncBar(this, labels(), selectedIndex, true, m_visualOpacity);
#endif
    } else {
        highlightAnim->stop();
        highlightAnim->setStartValue(startX);
        highlightAnim->setEndValue(endX);
        highlightAnim->start();
    }

    emit pageClicked(index);
    update();
}

void PostApplicationBar::setCurrentIndex(int index)
{
    if (index >= 0 && index < items.size()) {
        onItemClicked(index);
    }
}

void PostApplicationBar::onHighlightValueChanged(const QVariant& value)
{
    highlightX = value.toInt();
    updateSelectedRect();
    update();
}

void PostApplicationBar::onNativeSelectionChanged(int index)
{
    onItemClicked(index);
}

void PostApplicationBar::syncPlatformBar()
{
#ifdef Q_OS_MACOS
    if (!m_usesNativeBar) {
        return;
    }

    if (property(kSystemFloatingBarsSuppressedProperty).toBool()) {
        MacPostBarBridge::clearBar(this);
        return;
    }

    MacPostBarBridge::syncBar(this, labels(), selectedIndex, false, m_visualOpacity);
#endif
}

void PostApplicationBar::updatePanelShadow()
{
    if (m_usesNativeBar || shouldUseQtFallbackLiquidGlass()) {
        if (graphicsEffect()) {
            setGraphicsEffect(nullptr);
        }
        return;
    }

    auto* shadow = qobject_cast<QGraphicsDropShadowEffect*>(graphicsEffect());
    if (!shadow) {
        shadow = new QGraphicsDropShadowEffect(this);
        setGraphicsEffect(shadow);
    }

    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(ThemeManager::instance().color(ThemeColor::FloatingPanelShadow));
}

bool PostApplicationBar::shouldUseQtFallbackLiquidGlass() const
{
    return !m_usesNativeBar
            && ThemeManager::instance().postBarQtFallbackLiquidGlassEnabled();
}

void PostApplicationBar::updateQtFallbackLiquidGlassState()
{
    updatePanelShadow();
    if (m_liquidGlass) {
        m_liquidGlass->setEnabled(shouldUseQtFallbackLiquidGlass() && isVisible());
    }
}

void PostApplicationBar::releaseQtFallbackLiquidGlassResources(bool updateWidget)
{
    if (m_liquidGlass) {
        m_liquidGlass->release(updateWidget);
    }
}

QStringList PostApplicationBar::labels() const
{
    QStringList result;
    result.reserve(items.size());
    for (const TabItem& item : items) {
        result.push_back(item.label);
    }
    return result;
}
