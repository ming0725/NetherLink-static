#pragma once

#include "shared/ui/OverlayScrollListView.h"

#include <QModelIndex>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QString>
#include <QStyleOptionViewItem>

class AiChatMessageDelegate;
class QKeyEvent;
class QMouseEvent;

class AiChatMessageListView : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit AiChatMessageListView(QWidget* parent = nullptr);
    AiChatMessageDelegate* messageDelegate() const;
    void scrollToBottom();
    void clearTextSelection();
    void setBottomViewportMargin(int margin);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    int characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const;
    void copySelectionToClipboard();
    void selectAllTextInActiveBubble();
    void showSelectionMenu(const QPoint& globalPos);
    void showUrlMenu(const QPoint& globalPos, const QString& url);
    void openUrl(const QString& url);

    AiChatMessageDelegate* m_delegate = nullptr;
    QPersistentModelIndex m_activeBubbleIndex;
    QPersistentModelIndex m_dragIndex;
    int m_dragAnchor = -1;
    bool m_dragging = false;
    QPersistentModelIndex m_pressedUrlIndex;
    QPoint m_pressedUrlPos;
    QString m_pressedUrl;
};
