// MessageListWidget.h
#pragma once
#include "CustomScrollArea.h"
#include "MessageListItem.h"
#include <QVector>

class MessageListWidget : public CustomScrollArea {
    Q_OBJECT
    using CustomScrollArea::contentWidget;
public:
    explicit MessageListWidget(QWidget* parent = nullptr);
    void addMessage(const MessageItemContent& data);
    void clearMessages();
    MessageListItem* getSelectedItem() const { return selectItem; }
signals:
    void itemClicked(MessageListItem* item);
protected:
    void layoutContent() override;
private slots:
    void onItemClicked(MessageListItem*);
public slots:
    void onLastMessageUpdated(const QString& chatId, const QString& text, const QDateTime& timestamp);
private:
    MessageListItem* findItemById(const QString& id);
    QVector<MessageListItem*> m_items;
    MessageListItem* selectItem = nullptr;
};
