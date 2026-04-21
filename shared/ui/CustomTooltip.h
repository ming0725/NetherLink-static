#ifndef CUSTOMTOOLTIP_H
#define CUSTOMTOOLTIP_H

#include <QWidget>
#include <QLabel>

class CustomTooltip : public QWidget
{
    Q_OBJECT
public:
    explicit CustomTooltip(QWidget *parent = nullptr);
    void setText(const QString &text);
    void showTooltip(const QPoint &pos);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QLabel *m_label;
    static const int CORNER_RADIUS = 8;
    static const int PADDING = 8;
};

#endif // CUSTOMTOOLTIP_H 
