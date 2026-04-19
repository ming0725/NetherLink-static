#include "ChatListView.h"
#include <QWheelEvent>
#include <QScrollBar>
#include <QTimer>
#include <QPainter>
#include <QPropertyAnimation>
#include <QStyleOption>

ChatListView::ChatListView(QWidget *parent)
    : QListView(parent)
    , m_smoothScrollValue(0)
{
    setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    setFrameShape(QFrame::NoFrame);
    setSelectionMode(QAbstractItemView::NoSelection);
    setFocusPolicy(Qt::NoFocus);
    setViewportMargins(0, 0, 12, 0);
    
    // 创建自定义滚动条
    customScrollBar = new SmoothScrollBar(this);
    customScrollBar->hide();
    // 创建滚动动画
    scrollAnimation = new QPropertyAnimation(this, "smoothScrollValue", this);
    scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);
    scrollAnimation->setDuration(300);
    
    // 连接信号槽
    connect(customScrollBar, &SmoothScrollBar::valueChanged,
            this, &ChatListView::onCustomScrollValueChanged);
    connect(scrollAnimation, &QPropertyAnimation::finished,
            this, &ChatListView::onAnimationFinished);
}

void ChatListView::setModel(QAbstractItemModel *model)
{
    if (this->model()) {
        disconnect(this->model(), &QAbstractItemModel::rowsInserted,
                  this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::rowsRemoved,
                  this, &ChatListView::onModelRowsChanged);
        disconnect(this->model(), &QAbstractItemModel::modelReset,
                  this, &ChatListView::onModelRowsChanged);
    }

    QListView::setModel(model);

    if (model) {
        connect(model, &QAbstractItemModel::rowsInserted,
                this, &ChatListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::rowsRemoved,
                this, &ChatListView::onModelRowsChanged);
        connect(model, &QAbstractItemModel::modelReset,
                this, &ChatListView::onModelRowsChanged);
                
        // 初始化后延迟检查滚动条状态
        QTimer::singleShot(100, this, &ChatListView::checkScrollBarVisibility);
    }
}

void ChatListView::checkScrollBarVisibility()
{
    // 强制重新计算布局
    doItemsLayout();
    updateGeometries();
    
    // 检查是否需要滚动条
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    
    if (totalHeight <= viewport()->height()) {
        customScrollBar->hide();
        m_smoothScrollValue = 0;
        vScrollBar->setValue(0);
    } else {
        updateCustomScrollBar();
    }
}

void ChatListView::onModelRowsChanged()
{
    // 延迟检查滚动条状态
    QTimer::singleShot(50, this, &ChatListView::checkScrollBarVisibility);
    
    // 如果在底部，自动滚动到新位置
    QScrollBar* vScrollBar = verticalScrollBar();
    if (180 >= vScrollBar->maximum() - vScrollBar->value()) {
        QTimer::singleShot(50, this, &ChatListView::scrollToBottom);
    }
}

void ChatListView::mousePressEvent(QMouseEvent* event)
{
    QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        if (model()) {
            static_cast<ChatListModel*>(model())->clearSelection();
        }
    }
    QListView::mousePressEvent(event);
}

void ChatListView::resizeEvent(QResizeEvent *event)
{
    QListView::resizeEvent(event);
    
    // 更新自定义滚动条位置和大小
    customScrollBar->setGeometry(
        width() - customScrollBar->width(),  // 减小右边距为4像素
        0,
        customScrollBar->width(),
        height()
    );
    
    updateCustomScrollBar();
    
    // 强制重新计算所有项的大小和位置
    if (model()) {
        for (int i = 0; i < model()->rowCount(); ++i) {
            QModelIndex index = model()->index(i, 0);
            update(index);
        }
        doItemsLayout();
    }
}


void ChatListView::wheelEvent(QWheelEvent *event)
{
    if (model() && model()->rowCount() > 0) {
        // 检查是否真的需要滚动
        QScrollBar* vScrollBar = verticalScrollBar();
        int totalHeight = verticalScrollBar()->maximum() + verticalScrollBar()->pageStep();
        
        // 只有当内容高度大于视口高度时才允许滚动
        if (totalHeight <= viewport()->height()) {
            event->accept();
            return;
        }

        // 计算目标滚动值
        int delta = event->angleDelta().y();
        int targetValue = m_smoothScrollValue - delta;
        
        // 确保在有效范围内
        targetValue = qBound(0, targetValue, vScrollBar->maximum());
        
        // 开始滚动动画
        startScrollAnimation(targetValue);
        
        // 显示滚动条
        if (totalHeight > viewport()->height()) {
            customScrollBar->show();
            customScrollBar->showScrollBar();
        }
        
        event->accept();
    }
}

void ChatListView::onCustomScrollValueChanged(int value)
{
    if (scrollAnimation->state() != QPropertyAnimation::Running) {
        m_smoothScrollValue = value;
        verticalScrollBar()->setValue(value);
    }
}

void ChatListView::setSmoothScrollValue(int value)
{
    // 检查是否需要滚动
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();

    // 只在内容高度大于视口高度时更新滚动值
    if (totalHeight > viewport()->height()) {
        if (m_smoothScrollValue != value) {
            bool isAtBottom = value >= vScrollBar->maximum() - 5;

            m_smoothScrollValue = value;
            vScrollBar->setValue(value);

            if (!isAtBottom) {
                // 不在底部时才更新自定义滚动条的位置
                if (customScrollBar->isVisible()) {
                    customScrollBar->setValue(value);
                }
            }
        }
    }
}

void ChatListView::updateCustomScrollBar()
{
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    
    // 计算是否需要显示滚动条
    bool needScrollBar = totalHeight > viewport()->height();
    
    // 更新滚动条状态
    if (needScrollBar) {
        // 确保滚动条范围和步长正确
        customScrollBar->setRange(vScrollBar->minimum(), vScrollBar->maximum());
        customScrollBar->setPageStep(vScrollBar->pageStep());
        
        // 同步滚动条位置
        int currentValue = vScrollBar->value();
        customScrollBar->setValue(currentValue);
        m_smoothScrollValue = currentValue;
        
        customScrollBar->show();
    } else {
        customScrollBar->hide();
        m_smoothScrollValue = 0;
        vScrollBar->setValue(0);
    }
}

void ChatListView::startScrollAnimation(int targetValue)
{
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    
    if (totalHeight > viewport()->height()) {
        // 如果动画正在运行，先停止
        scrollAnimation->stop();
        
        // 计算滚动距离，据此调整动画时间
        int distance = qAbs(targetValue - m_smoothScrollValue);
        int duration = qMin(300, qMax(150, distance / 2));  // 最小150ms，最大300ms
        
        // 设置动画
        scrollAnimation->setDuration(duration);
        scrollAnimation->setStartValue(m_smoothScrollValue);
        scrollAnimation->setEndValue(targetValue);
        scrollAnimation->start();
    }
}

void ChatListView::onAnimationFinished()
{
    // 动画结束后更新滚动条状态
    updateCustomScrollBar();
}

bool ChatListView::viewportEvent(QEvent *event)
{
    // 处理所有视口相关的事件
    switch (event->type()) {
    case QEvent::Enter:
        if (customScrollBar->isVisible()) {
            customScrollBar->showScrollBar();
        }
        break;
    case QEvent::Leave: {
        QPoint globalPos = QCursor::pos();
        QPoint localPos = mapFromGlobal(globalPos);
        if (!rect().contains(localPos)) {
            customScrollBar->startFadeOut();
        }
        break;
    }
    case QEvent::MouseMove:
        if (customScrollBar->isVisible()) {
            customScrollBar->showScrollBar();
        }
        break;
    default:
        break;
    }
    return QListView::viewportEvent(event);
}

void ChatListView::enterEvent(QEnterEvent *event)
{
    QListView::enterEvent(event);
    hovered = true;
    if (customScrollBar->isVisible()) {
        customScrollBar->showScrollBar();
    }
}

void ChatListView::leaveEvent(QEvent *event)
{
    QListView::leaveEvent(event);
    QPoint globalPos = QCursor::pos();
    QPoint localPos = mapFromGlobal(globalPos);
    if (!rect().contains(localPos) && !customScrollBar->geometry().contains(localPos)) {
        customScrollBar->startFadeOut();
        hovered = false;
    }
}

int calculateDuration(int offset) {
    // 参数：可调
    const double minDuration = 200.0;   // 最快 120ms
    const double maxDuration = 800.0;   // 最慢 600ms
    // 控制曲线倾斜度，值越大远距离越快
    const double factor = 0.5;  // < 1 是减速函数，> 1 是加速函数
    // 归一化 offset
    double normalized = std::min(offset / 1000.0, 1.0); // 0~1之间（距离最多1000）
    // 使用指数衰减函数（效果柔和）
    double t = std::pow(normalized, factor);
    // 插值计算时间
    double duration = maxDuration * t;
    // 限幅，避免太快太慢
    return static_cast<int>(std::clamp(duration, minDuration, maxDuration));
}

void ChatListView::scrollToBottom()
{
    // 强制重新计算布局
    doItemsLayout();
    updateGeometries();
    
    QScrollBar* vScrollBar = verticalScrollBar();
    int totalHeight = vScrollBar->maximum() + vScrollBar->pageStep();
    
    const static int NEAR_BUTTOM_THRESHOLD = 250;

    if (totalHeight > viewport()->height()) {
        // 检查是否接近底部
        int currentBottom = vScrollBar->value() + viewport()->height();
        int offset = totalHeight - currentBottom;
        bool isNearBottom = offset <= NEAR_BUTTOM_THRESHOLD;

        // 停止当前动画
        scrollAnimation->stop();

        // 设置新的目标位置
        int targetValue = vScrollBar->maximum();

        // 设置动画
        scrollAnimation->setDuration(isNearBottom ? 200 : calculateDuration(offset));
        scrollAnimation->setEasingCurve(QEasingCurve::OutCubic);

        // 从当前位置开始动画
        scrollAnimation->setStartValue(vScrollBar->value());
        scrollAnimation->setEndValue(targetValue);

        // 同步滚动值
        m_smoothScrollValue = vScrollBar->value();

        if (isNearBottom && !hovered) {
            // 如果接近底部，不显示滚动条
            customScrollBar->hide();
        } else {
            // 不在底部附近，显示滚动条动画
            customScrollBar->show();
            customScrollBar->showScrollBar();
        }
        // 开始动画
        scrollAnimation->start();
    }
}
