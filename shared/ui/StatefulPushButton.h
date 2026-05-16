#pragma once

#include <QColor>
#include <QPropertyAnimation>
#include <QPushButton>

class StatefulPushButton : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(int radius READ radius WRITE setRadius)
    Q_PROPERTY(QColor normalColor READ normalColor WRITE setNormalColor)
    Q_PROPERTY(QColor hoverColor READ hoverColor WRITE setHoverColor)
    Q_PROPERTY(QColor pressColor READ pressColor WRITE setPressColor)
    Q_PROPERTY(QColor disabledColor READ disabledColor WRITE setDisabledColor)
    Q_PROPERTY(QColor currentColor READ currentColor WRITE setCurrentColor USER true)
    Q_PROPERTY(QColor textColor READ textColor WRITE setTextColor)
    Q_PROPERTY(Qt::Alignment textAlignment READ textAlignment WRITE setTextAlignment)
    Q_PROPERTY(QColor borderColor READ borderColor WRITE setBorderColor)
    Q_PROPERTY(int borderWidth READ borderWidth WRITE setBorderWidth)
    Q_PROPERTY(bool isFlat READ isFlat WRITE setFlat)

public:
    explicit StatefulPushButton(QWidget* parent = nullptr);
    explicit StatefulPushButton(const QString& text, QWidget* parent = nullptr);
    ~StatefulPushButton() override;

    int radius() const;
    void setRadius(int radius);

    QColor normalColor() const;
    void setNormalColor(const QColor& c);

    QColor hoverColor() const;
    void setHoverColor(const QColor& c);

    QColor pressColor() const;
    void setPressColor(const QColor& c);

    QColor disabledColor() const;
    void setDisabledColor(const QColor& c);

    QColor currentColor() const;
    void setCurrentColor(const QColor& c);

    QColor textColor() const;
    void setTextColor(const QColor& c);

    Qt::Alignment textAlignment() const;
    void setTextAlignment(Qt::Alignment alignment);

    QColor borderColor() const;
    void setBorderColor(const QColor& c);

    int borderWidth() const;
    void setBorderWidth(int w);

    bool isFlat() const;
    void setFlat(bool flat);

    void setAnimationDuration(int ms);
    int animationDuration() const;

    void setPressedVisual(bool pressed);

    void setDefaultStyle();
    void setPrimaryStyle();
    void setSuccessStyle();
    void setWarningStyle();
    void setDangerStyle();

protected:
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;

private:
    void initializeButton();
    void applyStateColor();
    void updateHoverState(bool hovered);
    void updatePressState(bool pressed);

    int m_radius;
    QColor m_normalColor;
    QColor m_hoverColor;
    QColor m_pressColor;
    QColor m_disabledColor;
    QColor m_currentColor;
    QColor m_textColor;
    Qt::Alignment m_textAlignment;
    QColor m_borderColor;
    int m_borderWidth;
    bool m_isFlat;
    int m_animationDuration;
    bool m_isHovered;
    bool m_isPressed;
    bool m_forcedPressedVisual;
    bool m_textColorFollowsBackground;
    QPropertyAnimation* m_colorAnimation;
};
