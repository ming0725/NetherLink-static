#pragma once
#include <QObject>
#include <QPoint>
#include <QString>
#include <QRect>
#include <QVariantAnimation>

class QPainter;

class ApplicationBarItem : public QObject {
    Q_OBJECT
public:
    ApplicationBarItem(const QString& normalSource, QObject* parent = nullptr);
    ApplicationBarItem(const QString& normalSource, const QString& selectedSource, QObject* parent = nullptr);

    void setPixmapScale(qreal scale);
    void setDarkModeInversionEnabled(bool enabled);
    void setSelected(bool);
    bool isSelected() const;

    void setHovered(bool hover);
    bool isHovered() const;

    void setRect(const QRect& rect);
    QRect rect() const;
    int y() const;
    bool contains(const QPoint& pos) const;

    void paint(QPainter& painter) const;
signals:
    void updateRequested();
private:
    qreal maxRippleRadius() const;

    QString selectedSource;
    QString normalSource;
    QRect itemRect;
    bool hovered = false;
    bool selected = false;
    bool darkModeInversionEnabled = true;
    qreal pixmapScale = 0.6;
    qreal rippleRadius = 0.0;
    QVariantAnimation* rippleAnim = nullptr;
};
