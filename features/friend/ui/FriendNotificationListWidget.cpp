#include "FriendNotificationListWidget.h"

#include <QMouseEvent>
#include <QScrollBar>

#include "features/friend/model/FriendNotificationListModel.h"
#include "features/friend/ui/FriendNotificationDelegate.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

FriendNotificationListWidget::FriendNotificationListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new FriendNotificationListModel(this))
    , m_delegate(new FriendNotificationDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::NoSelection);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setMouseTracking(true);
    viewport()->setMouseTracking(true);
    setUniformItemSizes(false);
    setSpacing(0);

    setThemeBackgroundRole(ThemeColor::PageBackground);
    refreshTheme();
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    connect(&ImageService::instance(), &ImageService::previewReady,
            viewport(), QOverload<>::of(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, [this]() { maybeRequestMore(); });
}

void FriendNotificationListWidget::setNotifications(QVector<FriendNotification> notifications)
{
    m_model->setNotifications(std::move(notifications));
}

void FriendNotificationListWidget::appendNotifications(QVector<FriendNotification> notifications)
{
    m_model->appendNotifications(std::move(notifications));
}

int FriendNotificationListWidget::notificationCount() const
{
    return m_model->notificationCount();
}

void FriendNotificationListWidget::maybeRequestMore()
{
    if (!verticalScrollBar() ||
        verticalScrollBar()->value() < verticalScrollBar()->maximum() - 160) {
        return;
    }

    emit loadMoreRequested();
}

void FriendNotificationListWidget::updateButtonHover(const QPoint& pos)
{
    const QModelIndex index = indexAt(pos);
    if (!index.isValid()) {
        if (m_hoveredIndex.isValid()) {
            m_model->setHoveredButton(m_hoveredIndex, -1);
            m_hoveredIndex = QPersistentModelIndex();
        }
        return;
    }

    QStyleOptionViewItem option;
    option.rect = visualRect(index);
    const int btn = m_delegate->buttonAt(option, index, pos);

    if (m_hoveredIndex.isValid() && m_hoveredIndex != index) {
        m_model->setHoveredButton(m_hoveredIndex, -1);
    }
    m_hoveredIndex = index;
    m_model->setHoveredButton(index, btn);
}

void FriendNotificationListWidget::mouseMoveEvent(QMouseEvent* event)
{
    updateButtonHover(event->pos());
    OverlayScrollListView::mouseMoveEvent(event);
}

void FriendNotificationListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const auto status = static_cast<NotificationStatus>(
        index.data(FriendNotificationListModel::StatusRole).toInt());
    if (status != NotificationStatus::Pending) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    QStyleOptionViewItem option;
    option.rect = visualRect(index);
    const int btn = m_delegate->buttonAt(option, index, event->pos());

    if (btn == 0 || btn == 1) {
        const QString notificationId = index.data(FriendNotificationListModel::IdRole).toString();
        if (btn == 0) {
            emit acceptRequest(notificationId);
        } else {
            emit rejectRequest(notificationId);
        }
        m_model->setHoveredButton(index, -1);
        m_hoveredIndex = QPersistentModelIndex();
        event->accept();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
}

void FriendNotificationListWidget::leaveEvent(QEvent* event)
{
    if (m_hoveredIndex.isValid()) {
        m_model->setHoveredButton(m_hoveredIndex, -1);
        m_hoveredIndex = QPersistentModelIndex();
    }
    OverlayScrollListView::leaveEvent(event);
}
