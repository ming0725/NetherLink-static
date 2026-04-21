#include "PostApplicationBar.h"

#include <QFontMetrics>
#include <QGraphicsBlurEffect>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsPixmapItem>
#include <QGraphicsScene>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>

PostApplicationBar::PostApplicationBar(QWidget* parent)
    : QWidget(parent)
    , m_parent(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    setMouseTracking(true);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(150, 150, 150, 220));
    setGraphicsEffect(shadow);

    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(300);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged,
            this, &PostApplicationBar::onHighlightValueChanged);

    initItems();
    resize(sizeHint());
    layoutItems();

    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &PostApplicationBar::updateBlurBackground);
    m_updateTimer->start(16);
    preSize = parent ? parent->size() : QSize();
}

void PostApplicationBar::enableBlur(bool enabled)
{
    isEnableBlur = enabled;
    if (!m_updateTimer) {
        return;
    }

    if (isEnableBlur) {
        if (!m_updateTimer->isActive()) {
            m_updateTimer->start(16);
        }
    } else {
        m_updateTimer->stop();
        m_blurredBackground = QImage();
        update();
    }
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
}

QSize PostApplicationBar::minimumSizeHint() const
{
    return sizeHint();
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
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    const QRectF contentRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal outerRadius = 15.0;

    if (!m_blurredBackground.isNull()) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(contentRect, outerRadius, outerRadius);
        painter.setClipPath(clipPath);
        painter.drawImage(rect(), m_blurredBackground);
        painter.setClipping(false);
    }

    painter.setBrush(QColor(255, 255, 255, 100));
    painter.drawRoundedRect(contentRect, outerRadius, outerRadius);

    if (!selectedRect.isEmpty()) {
        painter.setBrush(QColor(192, 192, 192, 192));
        painter.drawRoundedRect(selectedRect, 10, 10);
    }

    painter.setPen(Qt::black);
    for (const TabItem& item : items) {
        painter.drawText(item.rect, Qt::AlignCenter, item.label);
    }

    const qreal borderWidth = 2.0;
    const qreal inset = borderWidth / 2.0;
    const QRectF borderRect = rect().adjusted(inset, inset, -inset, -inset);
    QPen pen(QColor(0x00, 0x99, 0xff));
    pen.setWidthF(borderWidth);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(borderRect, outerRadius - inset, outerRadius - inset);
}

void PostApplicationBar::mouseMoveEvent(QMouseEvent* event)
{
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
    hoveredIndex = -1;
    unsetCursor();
    QWidget::leaveEvent(event);
}

void PostApplicationBar::mousePressEvent(QMouseEvent* event)
{
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
    highlightAnim->stop();
    selectedIndex = index;
    highlightAnim->setStartValue(startX);
    highlightAnim->setEndValue(endX);
    highlightAnim->start();
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

void PostApplicationBar::updateBlurBackground()
{
    if (!m_parent || !isVisible() || !isEnableBlur) {
        return;
    }

    QWidget* postApp = m_parent;
    if (m_parent->size() != preSize) {
        preSize = m_parent->size();
        m_blurredBackground = QImage(size(), QImage::Format_ARGB32);
        m_blurredBackground.fill(Qt::transparent);
        return;
    }

    QPixmap appShot(postApp->size());
    appShot.fill(Qt::transparent);
    const bool wasVisible = isVisible();
    hide();
    postApp->render(&appShot);
    if (wasVisible) {
        show();
    }

    const QRect myRect = geometry();
    const QPixmap backgroundShot = appShot.copy(myRect);

    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(backgroundShot);
    auto* blurEffect = new QGraphicsBlurEffect;
    blurEffect->setBlurRadius(10);
    item.setGraphicsEffect(blurEffect);
    scene.addItem(&item);

    m_blurredBackground = QImage(size(), QImage::Format_ARGB32);
    m_blurredBackground.fill(Qt::transparent);
    QPainter painter(&m_blurredBackground);
    scene.render(&painter, QRectF(), QRectF(0, 0, width(), height()));
    delete blurEffect;
}
