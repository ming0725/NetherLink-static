// smoothscrollwidget.h

#ifndef FRIENDLISTWIDGET_H
#define FRIENDLISTWIDGET_H

#include "FriendListItem.h"
#include <QWidget>
#include <QScrollArea>
#include <QVBoxLayout>
#include <QTimeLine>
#include "User.h"

class ScrollAreaNoWheel;
class ScrollBarThumb;

class FriendListWidget : public QWidget {
    Q_OBJECT
public:
    explicit FriendListWidget(QWidget *parent = nullptr);
    ~FriendListWidget();

    void addItem(const User& user);
    void removeItemAt(int index);
protected:
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onItemClicked(FriendListItem*);

private:
    void animateTo(int targetOffset);
    void updateScrollBar();
    void relayoutItems();

    ScrollAreaNoWheel *scrollArea = nullptr;
    ScrollBarThumb *scrollBarThumb = nullptr;
    QWidget *contentWidget = nullptr;
    QList<FriendListItem*> itemList;

    int contentOffset;   // 内容区域的偏移
    int thumbOffset;     // 滑块的偏移

    bool dragging;
    int dragStartY;
    int thumbOffsetAtDragStart;

    QTimeLine *scrollAnimation = nullptr;
    FriendListItem* selectItem = nullptr;
};

#endif // FRIENDLISTWIDGET_H
