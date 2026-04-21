#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QMouseEvent>

#include "OverlayScrollListView.h"
#include "ChatListModel.h"

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
