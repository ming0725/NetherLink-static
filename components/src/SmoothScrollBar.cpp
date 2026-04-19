#include "SmoothScrollBar.h"
#include <QPainter>
#include <QMouseEvent>

SmoothScrollBar::SmoothScrollBar(QWidget *parent)
    : QWidget(parent)
    , m_minimum(0)
    , m_maximum(0)
    , m_pageStep(0)
    , m_value(0)
    , m_opacity(0.0)
    , m_isDragging(false)
{
    setFixedWidth(8);  // 设置滚动条宽度
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_TransparentForMouseEvents, false);  // 确保可以接收鼠标事件
    
    // 初始化淡入淡出动画
    m_fadeAnimation = new QPropertyAnimation(this, "opacity", this);
    m_fadeAnimation->setDuration(200);  // 加快动画速度
    
    // 初始化淡出计时器
    m_fadeOutTimer = new QTimer(this);
    m_fadeOutTimer->setSingleShot(true);
    connect(m_fadeOutTimer, &QTimer::timeout, this, &SmoothScrollBar::startFadeOut);
}

void SmoothScrollBar::setRange(int min, int max)
{
    m_minimum = min;
    m_maximum = max;
    update();
}

void SmoothScrollBar::setPageStep(int step)
{
    m_pageStep = step;
    update();
}

void SmoothScrollBar::setValue(int value)
{
    value = qBound(m_minimum, value, m_maximum);
    if (m_value != value) {
        m_value = value;
        emit valueChanged(value);
        update();
    }
}

void SmoothScrollBar::setOpacity(qreal opacity)
{
    if (m_opacity != opacity) {
        m_opacity = opacity;
        update();
    }
}

void SmoothScrollBar::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event);
    if (m_maximum <= m_minimum) return;

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    // 只绘制滚动条滑块，不绘制背景轨道
    painter.setOpacity(m_opacity);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(128, 128, 128, 80));  // 使用灰色半透明
    painter.drawRoundedRect(getHandleRect(), 4, 4);
}

QRect SmoothScrollBar::getHandleRect() const
{
    if (m_maximum <= m_minimum) return QRect();

    qreal visibleRatio = qreal(m_pageStep) / (m_maximum - m_minimum + m_pageStep);
    int handleHeight = qMax(static_cast<int>(height() * visibleRatio), 30);
    
    qreal valueRatio = qreal(m_value - m_minimum) / (m_maximum - m_minimum);
    int handleY = valueRatio * (height() - handleHeight);
    
    return QRect(0, handleY, width(), handleHeight);
}

void SmoothScrollBar::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_isDragging = true;
        m_dragStartPosition = event->pos();
        m_dragStartValue = m_value;
        updateValue(event->pos());
    }
}

void SmoothScrollBar::mouseMoveEvent(QMouseEvent *event)
{
    if (m_isDragging) {
        updateValue(event->pos());
    }
}

void SmoothScrollBar::mouseReleaseEvent(QMouseEvent *event)
{
    Q_UNUSED(event);
    m_isDragging = false;
    if (!rect().contains(mapFromGlobal(QCursor::pos()))) {
        startFadeOut();  // 如果鼠标不在滚动条区域内，立即开始淡出
    }
}

void SmoothScrollBar::enterEvent(QEnterEvent *event)
{
    Q_UNUSED(event);
    showScrollBar();
    m_fadeOutTimer->stop();  // 停止任何正在进行的淡出计时
}

void SmoothScrollBar::leaveEvent(QEvent *event)
{
    Q_UNUSED(event);
    if (!m_isDragging) {
        startFadeOut();  // 立即开始淡出
    }
}

void SmoothScrollBar::updateValue(const QPoint &pos)
{
    if (m_maximum <= m_minimum) return;

    QRect handleRect = getHandleRect();
    int handleHeight = handleRect.height();
    qreal valueRatio = qreal(pos.y() - m_dragStartPosition.y()) / (height() - handleHeight);
    int valueDelta = valueRatio * (m_maximum - m_minimum);
    setValue(m_dragStartValue + valueDelta);
}

void SmoothScrollBar::showScrollBar()
{
    m_fadeOutTimer->stop();
    m_fadeAnimation->stop();
    m_fadeAnimation->setStartValue(m_opacity);
    m_fadeAnimation->setEndValue(1.0);
    m_fadeAnimation->start();
}

void SmoothScrollBar::startFadeOut()
{
    if (!m_isDragging) {
        m_fadeAnimation->stop();
        m_fadeAnimation->setStartValue(m_opacity);
        m_fadeAnimation->setEndValue(0.0);
        m_fadeAnimation->start();
    }
} 
