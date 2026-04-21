#pragma once
#include <QWidget>
#include <QString>
#include <QEvent>
#include <QPropertyAnimation>

class ApplicationBarItem : public QWidget {
    Q_OBJECT
public:
    ApplicationBarItem(const QString& normalSource, QWidget* parent = nullptr);
    ApplicationBarItem(const QString& normalSource, const QString& selectedSource, QWidget* parent = nullptr);
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
    QString selectedSource;
    QString normalSource;
    bool hoverd = false;
    bool selected = false;
    qreal pixmapScale = 0.6;
    qreal rippleRadius = 0.0;
    QVariantAnimation* rippleAnim = nullptr;
};

