#include "FriendListWidget.h"
#include "ScrollBarThumb.h"
#include "ScrollAreaNoWheel.h"
#include "UserRepository.h"
#include <QPainter>
#include <QMouseEvent>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QTimer>
#include <QCollator>

static bool friendItemLessThan(FriendListItem* a, FriendListItem* b) {
    // 非离线的排前面
    if (a->getUserStatus() != Offline && b->getUserStatus() == Offline)
        return true;
    if (a->getUserStatus() == Offline && b->getUserStatus() != Offline)
        return false;

    // 组内中文姓名排序，使用 QCollator 支持中文排序
    static QCollator collator;
    collator.setLocale(QLocale::Chinese); // 使用中文排序规则
    collator.setNumericMode(true);
    return collator.compare(a->getUserName(), b->getUserName()) < 0;
}

FriendListWidget::FriendListWidget(QWidget *parent)
    : QWidget(parent)
    , scrollArea(new ScrollAreaNoWheel(this))
    , scrollBarThumb(new ScrollBarThumb(this))
    , contentWidget(new QWidget)
    , contentOffset(0)
    , thumbOffset(0)
    , dragging(false)
    , dragStartY(0)
    , thumbOffsetAtDragStart(0)
    , scrollAnimation(new QTimeLine(500, this))
{
    scrollBarThumb->installEventFilter(this);
    this->installEventFilter(this);  // 滚动容器自己也要监控
    scrollArea->viewport()->installEventFilter(this);
    contentWidget->installEventFilter(this);
    auto *opacity = new QGraphicsOpacityEffect(scrollBarThumb);
    opacity->setOpacity(0.0);
    scrollBarThumb->setGraphicsEffect(opacity);
    scrollBarThumb->hide();

    auto allUser = UserRepository::instance().getAllUser();
    for (int i = 0; i < allUser.size(); ++i) {
        addItem(allUser[i]);
    }

    scrollArea->setWidget(contentWidget);
    scrollArea->setGeometry(rect());

    scrollBarThumb->setColor(QColor(0x3f3f3f));
    scrollBarThumb->installEventFilter(this);

    scrollAnimation->setEasingCurve(QEasingCurve::OutBack);
    scrollAnimation->setUpdateInterval(0);
    connect(scrollAnimation, &QTimeLine::frameChanged, this, [=](int value) {
        int maxContentOffset = contentWidget->height() - height();
        contentOffset = qBound(0, value, maxContentOffset);
        contentWidget->move(0, -contentOffset);

        int maxThumbOffset = height() - scrollBarThumb->height();
        thumbOffset = contentOffset * maxThumbOffset / maxContentOffset;
        scrollBarThumb->move(width() - 13, thumbOffset);
    });
    updateScrollBar();
    std::sort(itemList.begin(), itemList.end(), friendItemLessThan);
}

FriendListWidget::~FriendListWidget()
{
}

void FriendListWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    scrollArea->setGeometry(rect());
    relayoutItems(); // 每次尺寸变化都重新排布子项
}


void FriendListWidget::wheelEvent(QWheelEvent *event) {
    // 内容不足一页时，不滚动，否则可能导致崩溃
    if (contentWidget->height() <= height()) {
        event->ignore();
        return;
    }
    // 正常滚动逻辑
    const int delta = -event->angleDelta().y();
    animateTo(contentOffset + (delta>0?240:-240));
    event->accept();
}

void FriendListWidget::addItem(const User& user)
{
    auto *item = new FriendListItem(user, contentWidget);
    itemList.append(item);
    item->show();
    connect(item, &FriendListItem::itemClicked, this, &FriendListWidget::onItemClicked);
    //connect(item, &FriendListItem::customContextMenuRequested, this, &FriendListWidget::onItemRightClicked);
    relayoutItems();
}

void FriendListWidget::relayoutItems()
{
    // 1) 重新排列所有子项
    int y = 0;
    for (auto *item : itemList) {
        item->move(0, y);
        item->resize(width(), item->sizeHint().height());
        y += item->height();
    }

    // 2) 调整 contentWidget 的总高度
    contentWidget->resize(width(), y);

    // 3) 修正 contentOffset（防止越界）
    int maxContentOffset = qMax(0, y - height());
    contentOffset = qBound(0, contentOffset, maxContentOffset);

    // 4) 重新移动内容区以恢复“滚动”位置
    contentWidget->move(0, -contentOffset);  // 等同于 QScrollArea 的 updateWidgetPosition()&#8203;:contentReference[oaicite:3]{index=3}

    // 5) 重新计算滑块高度和位置
    int thumbHeight = qBound(qMin(20, height()), height() * height() / qMax(1, y), qMax(20, height()));
    int maxThumbOffset = height() - thumbHeight;

    // 6) 根据 contentOffset 计算 thumbOffset 并移动滑块
    thumbOffset = (maxContentOffset > 0)
                      ? contentOffset * maxThumbOffset / maxContentOffset
                      : 0;

    scrollBarThumb->setGeometry(width() - 13, thumbOffset, 8, thumbHeight);
}


bool FriendListWidget::eventFilter(QObject *obj, QEvent *event)
{
    if (event->type() == QEvent::Enter) {
        int contentHeight = contentWidget->height();
        int viewportHeight = height();

        // 当内容不需滚动时，隐藏 thumb 并返回
        if (contentHeight <= viewportHeight) {
            scrollBarThumb->hide();
        }
        else {
            scrollBarThumb->show();
            scrollBarThumb->setVisible(contentHeight > viewportHeight);
            auto *anim = new QPropertyAnimation(scrollBarThumb->graphicsEffect(), "opacity", this);
            anim->setDuration(200);
            anim->setStartValue(scrollBarThumb->graphicsEffect()->property("opacity").toDouble());
            anim->setEndValue(1.0);
            anim->start(QAbstractAnimation::DeleteWhenStopped);
        }
    }
    else if (event->type() == QEvent::Leave) {
        auto *anim = new QPropertyAnimation(scrollBarThumb->graphicsEffect(), "opacity", this);
        anim->setDuration(200);
        anim->setStartValue(scrollBarThumb->graphicsEffect()->property("opacity").toDouble());
        anim->setEndValue(0.0);
        connect(anim, &QPropertyAnimation::finished, scrollBarThumb, &QWidget::hide);
        anim->start(QAbstractAnimation::DeleteWhenStopped);
    }
    else if (obj == scrollBarThumb) {
        if (event->type() == QEvent::MouseButtonPress) {
            auto *me = static_cast<QMouseEvent*>(event);
            if (me->button() == Qt::LeftButton) {
                dragging = true;
                dragStartY = me->globalPosition().y();
                thumbOffsetAtDragStart = thumbOffset;
                return true;
            }
        } else if (event->type() == QEvent::MouseMove && dragging && scrollBarThumb->isVisible()) {
            auto *me = static_cast<QMouseEvent*>(event);
            int deltaY = me->globalPosition().y() - dragStartY;
            int maxThumbOffset = height() - scrollBarThumb->height();
            thumbOffset = qBound(0, thumbOffsetAtDragStart + deltaY, maxThumbOffset);

            int maxContentOffset = contentWidget->height() - height();
            contentOffset = thumbOffset * maxContentOffset / maxThumbOffset;

            contentWidget->move(0, -contentOffset);
            scrollBarThumb->move(width() - 13, thumbOffset);
            return true;
        } else if (event->type() == QEvent::MouseButtonRelease && dragging) {
            dragging = false;
            return true;
        }
    }

    return QWidget::eventFilter(obj, event);
}

void FriendListWidget::animateTo(int targetOffset)
{
    int maxOffset = contentWidget->height() - height();
    // 无滚动空间时直接返回
    if (maxOffset <= 0) return;
    scrollAnimation->stop();
    targetOffset = qBound(0, targetOffset, maxOffset);
    scrollAnimation->setFrameRange(contentOffset, targetOffset);
    scrollAnimation->start();
}


void FriendListWidget::updateScrollBar()
{
    int contentHeight = contentWidget->height();
    int viewportHeight = height();

    // 当内容不需滚动时，隐藏 thumb 并返回
    if (contentHeight <= viewportHeight) {
        scrollBarThumb->setVisible(false);
        return;
    }
    // 否则显示并正常计算 thumb 大小和位置
    scrollBarThumb->show();
    int thumbHeight = viewportHeight * viewportHeight / contentHeight;
    thumbHeight = qBound(20, thumbHeight, viewportHeight);
    scrollBarThumb->setGeometry(width() - 13, thumbOffset, 8, thumbHeight);
}


void FriendListWidget::removeItemAt(int index)
{
    if (index < 0 || index >= itemList.size()) return;
    FriendListItem* item = itemList.takeAt(index);
    item->deleteLater();
    scrollAnimation->stop();
    relayoutItems();
}

void FriendListWidget::onItemClicked(FriendListItem* item)
{
    if (selectItem) {
        selectItem->setSelected(false);
    }
    if (item) {
        if (item == selectItem) {
            item->setSelected(false);
            selectItem = nullptr;
        }
        else {
            item->setSelected(true);
            selectItem = item;
        }
    }
}

