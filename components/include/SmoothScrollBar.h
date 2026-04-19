#ifndef SMOOTHSCROLLBAR_H
#define SMOOTHSCROLLBAR_H

#include <QWidget>
#include <QPropertyAnimation>
#include <QTimer>

class SmoothScrollBar : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(int value READ value WRITE setValue)
    Q_PROPERTY(qreal opacity READ opacity WRITE setOpacity)

public:
    explicit SmoothScrollBar(QWidget *parent = nullptr);

    void setRange(int min, int max);
    void setPageStep(int step);
    int value() const { return m_value; }
    qreal opacity() const { return m_opacity; }

public slots:
    void setValue(int value);
    void setOpacity(qreal opacity);
    void startFadeOut();
    void showScrollBar();

signals:
    void valueChanged(int value);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    int m_minimum;
    int m_maximum;
    int m_pageStep;
    int m_value;
    qreal m_opacity;
    bool m_isDragging;
    QPoint m_dragStartPosition;
    int m_dragStartValue;
    
    QPropertyAnimation *m_fadeAnimation;
    QTimer *m_fadeOutTimer;

    QRect getHandleRect() const;

    void updateValue(const QPoint &pos);
};

#endif // SMOOTHSCROLLBAR_H 