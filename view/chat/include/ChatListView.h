#ifndef CHATLISTVIEW_H
#define CHATLISTVIEW_H

#include <QListView>
#include <QScrollBar>
#include <QMouseEvent>
#include <QPropertyAnimation>
#include "ChatListModel.h"
#include "SmoothScrollBar.h"

class ChatListView : public QListView
{
    Q_OBJECT
    Q_PROPERTY(int smoothScrollValue READ smoothScrollValue WRITE setSmoothScrollValue)
public:
    explicit ChatListView(QWidget *parent = nullptr);
    void setModel(QAbstractItemModel *model) override;
    void scrollToBottom();

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;
    bool viewportEvent(QEvent *event) override;

private slots:
    void onCustomScrollValueChanged(int value);
    void onAnimationFinished();
    void onModelRowsChanged();
    void checkScrollBarVisibility();

private:
    SmoothScrollBar *customScrollBar;
    QPropertyAnimation *scrollAnimation;
    int m_smoothScrollValue;
    bool hovered = false;
    
    int smoothScrollValue() const { return m_smoothScrollValue; }
    void setSmoothScrollValue(int value);
    void updateCustomScrollBar();
    void startScrollAnimation(int targetValue);
};

#endif // CHATLISTVIEW_H 
