// MessageListWidget.cpp
#include "MessageListWidget.h"
#include "MessageRepository.h"
#include "UserRepository.h"
#include "GroupRepository.h"
#include <QEvent>
#include <algorithm>

MessageListWidget::MessageListWidget(QWidget* parent)
    : CustomScrollArea(parent)
{
    installEventFilter(this);
    setMouseTracking(true);
    auto& mr = MessageRepository::instance();
    auto& gr = GroupRepository::instance();
    auto& ur = UserRepository::instance();
    auto groups = GroupRepository::instance().getAllGroup();
    for (auto& group : groups) {
        auto lastMsg = mr.getLastMessage(group.groupId);
        auto lastContent = lastMsg->getSenderName() + "：" + lastMsg->getContent();
        auto lastTime = lastMsg->getTimestamp();
        int unreadCount = mr.getMessages(group.groupId).count();
        bool isDnd = gr.getGroup(group.groupId).isDnd;
        auto name = QString("%1（%2）").arg(group.groupName, QString::number(group.memberNum));
        MessageItemContent mic{
                name,
                lastContent,
                lastTime,
                unreadCount,
                isDnd,
                group.groupAvatarPath,
                group.groupId,
                true
        };
        addMessage(mic);
    }

    // 单聊初始消息
    auto users = UserRepository::instance().getAllUser();
    for (auto user : users) {
        auto lastMsg = mr.getLastMessage(user.id);
        auto lastContent = lastMsg->getContent();
        auto lastTime = lastMsg->getTimestamp();
        int unreadCount = mr.getMessages(user.id).count();
        bool isDnd = ur.getUser(user.id).isDnd;
        auto name = user.nick;
        MessageItemContent mic{
                name,
                lastContent,
                lastTime,
                unreadCount,
                isDnd,
                user.avatarPath,
                user.id
        };
        addMessage(mic);
    }
    std::sort(m_items.begin(), m_items.end(),
          [](MessageListItem* a, MessageListItem* b) {
              return a->getLastTime() > b->getLastTime();
    });
}

void MessageListWidget::addMessage(const MessageItemContent& data) {
    auto *it = new MessageListItem(data, contentWidget);
    connect(it, &MessageListItem::itemClicked,
            this, &MessageListWidget::onItemClicked);
    m_items.append(it);
}

void MessageListWidget::clearMessages() {
    for (auto *it : m_items) {
        delete it;
    }
    m_items.clear();
}

void MessageListWidget::layoutContent() {
    if (!std::is_sorted(m_items.begin(), m_items.end(),
    [](MessageListItem* a, MessageListItem* b) { return a->getLastTime() > b->getLastTime(); }))
    {
        std::sort(m_items.begin(), m_items.end(),
              [](MessageListItem* a, MessageListItem* b) {
                  return a->getLastTime() > b->getLastTime();
        });
    }
    int y = 0;
    int w = contentWidget->width();
    for (auto *it : m_items) {
        int h = it->sizeHint().height();
        it->setGeometry(0, y, w, h);
        y += h;
    }
    // 最后调整 contentWidget 的高度，让基类能正确计算滚动范围
    contentWidget->resize(w, y);
}

void MessageListWidget::onItemClicked(MessageListItem* item) {
    if (!item) return;
    if (item != selectItem) {
        emit itemClicked(item);
    }
    if (selectItem) {
        selectItem->setSelected(false);
        item->setSelected(true);
        selectItem = item;
    }
    else {
        selectItem = item;
        selectItem->setSelected(true);
    }
}

void MessageListWidget::onLastMessageUpdated(const QString& chatId,
                                             const QString& text,
                                             const QDateTime& timestamp)
{
    MessageListItem* item = findItemById(chatId);
    if (item) {
        item->setLastTime(timestamp);
        item->setLastText(text);
    }
    // 按时间排序
    std::sort(m_items.begin(), m_items.end(),
              [](MessageListItem* a, MessageListItem* b) {
                  return a->getLastTime() > b->getLastTime();
              });
    layoutContent();
}

MessageListItem* MessageListWidget::findItemById(const QString& id)
{
    for (auto item : m_items) {
        if (item->getChatID() == id) {
            return item;
        }
    }
    return nullptr;
}
