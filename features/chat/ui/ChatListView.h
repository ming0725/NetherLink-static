#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QMouseEvent>

#include "shared/ui/OverlayScrollListView.h"
#include "features/chat/model/ChatListModel.h"

class ChatListView : public OverlayScrollListView
{
    Q_OBJECT
public:
    explicit ChatListView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    void scrollToBottom();

protected:
    void mousePressEvent(QMouseEvent* event) override;

private slots:
    void onModelRowsChanged();

private:
    static constexpr int kAutoScrollThreshold = 180;
};

#endif // CHATLISTVIEW_H 
