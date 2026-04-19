#pragma once

#include "CustomScrollArea.h"
#include "AiChatListItem.h"

class AiChatListWidget : public CustomScrollArea {
    Q_OBJECT
public:
    explicit AiChatListWidget(QWidget* parent = nullptr);

    void addChatItem(AiChatListItem*);

signals:
    void chatItemClicked(AiChatListItem* item);
    void chatItemRenamed(AiChatListItem* item);
    void chatItemDeleted(AiChatListItem* item);
    void chatOrderChanged();

protected:
    void layoutContent() override;

private slots:
    void onItemClicked(AiChatListItem*);

private:
    // 布局间距
    const int topMargin = 10;
    const int leftMargin = 10;
    const int iconHeight = 24;
    const int iconTextSpacing = 5;
    const int itemSpacing = 2;
    const int sectionSpacing = 0;
    const int groupSpacing = 8;

    QWidget* content;
    QPushButton* newChatButton;

    // 仅保存对话项，不包含 QLabel
    QVector<AiChatListItem*> m_items;

    QString timeSectionText(const QDateTime& dt) const;
    AiChatListItem* selectedItem = nullptr;
};
