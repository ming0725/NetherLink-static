#pragma once
#include <QWidget>
#include <QPixmap>
#include <QEvent>
#include <QPropertyAnimation>

class ApplicationBarItem : public QWidget {
    Q_OBJECT
public:
    ApplicationBarItem(QPixmap normal, QWidget* parent = nullptr);
    ApplicationBarItem(QPixmap normal, QPixmap selected, QWidget* parent = nullptr);
    void setPixmapScale(qreal scale);
    void setSelected(bool);
    bool isSelected();
protected:
    void enterEvent(QEnterEvent*) Q_DECL_OVERRIDE;
    void leaveEvent(QEvent*) Q_DECL_OVERRIDE;
    void paintEvent(QPaintEvent*) Q_DECL_OVERRIDE;
    void mousePressEvent(QMouseEvent*) Q_DECL_OVERRIDE;
signals:
    void itemClicked(ApplicationBarItem* item);
private:
    QPixmap selectedPixmap;
    QPixmap normalPixmap;
    bool hoverd = false;
    bool selected = false;
    qreal pixmapScale = 0.6;
    qreal rippleRadius = 0.0;
    QVariantAnimation* rippleAnim = nullptr;
};


