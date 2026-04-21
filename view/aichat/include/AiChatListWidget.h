#pragma once

#include "OverlayScrollListView.h"
#include "AiChatListModel.h"

#include <QString>
#include <QStyleOptionViewItem>

class AiChatListDelegate;
class AiChatListModel;
class QMouseEvent;
class QPaintEvent;
class QVariantAnimation;
class QModelIndex;

class AiChatListWidget : public OverlayScrollListView
{
public:
    explicit AiChatListWidget(QWidget* parent = nullptr);
    void ensureInitialized();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    void reloadEntries(const QString& selectedConversationId = {});
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void updateStickyHeader();
    struct StickyHeaderState {
        bool visible = false;
        QString title;
        int offsetY = 0;
    };

    StickyHeaderState calculateStickyHeaderState() const;
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    void showItemMenu(const QModelIndex& index);
    void renameItem(const QModelIndex& index);
    void deleteItem(const QModelIndex& index);
    void drawStickyHeader() const;

    AiChatListModel* m_model;
    AiChatListDelegate* m_delegate;
    QVariantAnimation* m_stickyAnimation;
    QString m_stickyTitle;
    QString m_previousStickyTitle;
    qreal m_stickyTransitionProgress = 1.0;
    bool m_stickyVisible = false;
    int m_stickyOffsetY = 0;
    bool m_initialized = false;
};
