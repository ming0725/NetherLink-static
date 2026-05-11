#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QMouseEvent>
#include <QPersistentModelIndex>
#include <QPoint>
#include <QPropertyAnimation>
#include <QStyleOptionViewItem>

#include "shared/ui/OverlayScrollListView.h"
#include "features/chat/model/ChatListModel.h"

class ChatItemDelegate;
class QKeyEvent;

class ChatListView : public OverlayScrollListView
{
    Q_OBJECT
public:
    explicit ChatListView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    void scrollToBottom(bool accelerateFarDistance = false);
    void jumpToBottom();
    void preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum);
    void clearTextSelection();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private slots:
    void onModelRowsChanged();

private:
    ChatItemDelegate* chatDelegate() const;
    QStyleOptionViewItem viewOptionForIndex(const QModelIndex& index) const;
    int characterIndexForDrag(const QModelIndex& index, const QPoint& pos) const;
    void copySelectionToClipboard();
    void selectAllTextInActiveBubble();
    void showSelectionMenu(const QPoint& globalPos);
    void showUrlMenu(const QPoint& globalPos, const QString& url);
    void openUrl(const QString& url);

    QPropertyAnimation* m_scrollAnimation;
    QPersistentModelIndex m_activeBubbleIndex;
    QPersistentModelIndex m_dragIndex;
    int m_dragAnchor = -1;
    bool m_dragging = false;
    QPersistentModelIndex m_pressedUrlIndex;
    QPoint m_pressedUrlPos;
    QString m_pressedUrl;
};

#endif // CHATLISTVIEW_H 
