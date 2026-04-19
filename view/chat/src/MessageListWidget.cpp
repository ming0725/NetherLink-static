// MessageListWidget.cpp
#include "MessageListWidget.h"
#include "MessageRepository.h"
#include <QEvent>
#include <algorithm>

MessageListWidget::MessageListWidget(QWidget* parent)
    : CustomScrollArea(parent)
{
    installEventFilter(this);
    setMouseTracking(true);
    const QVector<ConversationSummary> conversations =
            MessageRepository::instance().requestConversationList();
    for (const ConversationSummary& conversation : conversations) {
        addMessage(conversation);
    }
    std::sort(m_items.begin(), m_items.end(),
          [](MessageListItem* a, MessageListItem* b) {
              return a->getLastTime() > b->getLastTime();
    });
}

void MessageListWidget::addMessage(const ConversationSummary& data) {
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
