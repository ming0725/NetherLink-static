#ifndef NEWMESSAGENOTIFIER_H
#define NEWMESSAGENOTIFIER_H

#include <QWidget>
#include <QLabel>

class NewMessageNotifier : public QWidget
{
    Q_OBJECT

public:
    explicit NewMessageNotifier(QWidget *parent = nullptr);
    void setMessageCount(int count);

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QLabel *label;
    void updateText(int count);
    void updateLayout();
};

#endif // NEWMESSAGENOTIFIER_H 