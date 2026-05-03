#pragma once

#include <QMargins>
#include <QString>
#include <QWidget>

class QMouseEvent;
class QPainter;
class QPaintEvent;

class MinecraftSlider final : public QWidget
{
    Q_OBJECT

public:
    explicit MinecraftSlider(QWidget* parent = nullptr);

    void setRange(int minimum, int maximum);
    void setValue(int value);
    void setNormalImage(const QString& source);
    void setHandleImage(const QString& source);
    void setHandleHoverImage(const QString& source);
    void setSourceMargins(const QMargins& margins);
    void setText(const QString& text);

    int value() const;

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void valueChanged(int value);

protected:
    bool event(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    QRect handleRect() const;
    int handleWidth() const;
    int valueFromPosition(int x) const;
    void setHovered(bool hovered);
    void drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const;

    QString m_normalImage;
    QString m_handleImage;
    QString m_handleHoverImage;
    QString m_text;
    QMargins m_sourceMargins;
    int m_minimum = 0;
    int m_maximum = 100;
    int m_value = 0;
    bool m_hovered = false;
    bool m_dragging = false;
};
