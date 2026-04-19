#pragma once
#include <QWidget>
#include <QScrollArea>
#include <QTimeLine>
#include <QList>

class ScrollAreaNoWheel;
class ScrollBarThumb;

class SmoothScrollWidget : public QWidget {
    Q_OBJECT
public:
    explicit SmoothScrollWidget(QWidget *parent = nullptr);
    virtual ~SmoothScrollWidget();

protected:
    void wheelEvent(QWheelEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;
    virtual void animateTo(int targetOffset);
    virtual void updateScrollBar();
    virtual void relayoutContent();
    virtual void setContentMinimumHeight(int height);  // 提供给子类设置内容高度（用于 setGeometry 布局）
    // 子类通过该 widget 添加内容即可，基类会处理滚动逻辑
    QWidget *contentWidget;
    ScrollAreaNoWheel *scrollArea;
    ScrollBarThumb *scrollBarThumb;
    QTimeLine *scrollAnimation;
    int contentOffset;
    int thumbOffset;
    bool dragging;
    int dragStartY;
    int thumbOffsetAtDragStart;
};
