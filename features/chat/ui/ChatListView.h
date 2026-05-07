#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QMouseEvent>
#include <QPropertyAnimation>

#include "shared/ui/OverlayScrollListView.h"
#include "features/chat/model/ChatListModel.h"

class ChatListView : public OverlayScrollListView
{
    Q_OBJECT
public:
    explicit ChatListView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    void scrollToBottom(bool accelerateFarDistance = false);
    void jumpToBottom();
    void preserveScrollPositionAfterPrepend(int previousValue, int previousMaximum);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onModelRowsChanged();

private:
    QPropertyAnimation* m_scrollAnimation;
};

#endif // CHATLISTVIEW_H 
