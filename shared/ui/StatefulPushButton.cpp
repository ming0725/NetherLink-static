#include "StatefulPushButton.h"

#include <QMouseEvent>
#include <QHoverEvent>
#include <QPainter>
#include <QPainterPath>
#include <QFontMetrics>

StatefulPushButton::StatefulPushButton(QWidget* parent)
        : QPushButton(parent)
        , m_radius(8)
        , m_normalColor(0x0099ff)
        , m_hoverColor(0x0088ee)
        , m_pressColor(0x0077dd)
        , m_disabledColor(0xa0a0a0)
        , m_currentColor(m_normalColor)
        , m_textColor(Qt::white)
        , m_borderColor(Qt::transparent)
        , m_borderWidth(0)
        , m_isFlat(false)
        , m_animationDuration(0)
        , m_isHovered(false)
        , m_isPressed(false)
        , m_colorAnimation(new QPropertyAnimation(this, "currentColor"))
{
    initializeButton();
}

StatefulPushButton::StatefulPushButton(const QString& text, QWidget* parent)
        : QPushButton(text, parent)
        , m_radius(8)
        , m_normalColor(0x0099ff)
        , m_hoverColor(0x0088ee)
        , m_pressColor(0x0077dd)
        , m_disabledColor(0xa0a0a0)
        , m_currentColor(m_normalColor)
        , m_textColor(Qt::white)
        , m_borderColor(Qt::transparent)
        , m_borderWidth(0)
        , m_isFlat(false)
        , m_animationDuration(0)
        , m_isHovered(false)
        , m_isPressed(false)
        , m_colorAnimation(new QPropertyAnimation(this, "currentColor"))
{
    initializeButton();
}

StatefulPushButton::~StatefulPushButton()
{
    delete m_colorAnimation;
}

void StatefulPushButton::initializeButton()
{
    setMouseTracking(true);
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_MacShowFocusRect, false);
    setFocusPolicy(Qt::NoFocus);
    setAutoDefault(false);
    setDefault(false);

    m_colorAnimation->setDuration(m_animationDuration);
    m_colorAnimation->setEasingCurve(QEasingCurve::OutCubic);

    applyStateColor();
}

void StatefulPushButton::applyStateColor()
{
    QColor target;
    if (!isEnabled()) {
        target = m_disabledColor;
    } else if (m_isPressed) {
        target = m_pressColor;
    } else if (m_isHovered) {
        target = m_hoverColor;
    } else {
        target = m_normalColor;
    }

    if (m_currentColor == target) {
        return;
    }

    m_colorAnimation->stop();
    setCurrentColor(target);
}

void StatefulPushButton::updateHoverState(bool hovered)
{
    if (m_isHovered == hovered) {
        return;
    }

    m_isHovered = hovered;
    applyStateColor();
}

void StatefulPushButton::updatePressState(bool pressed)
{
    if (m_isPressed == pressed) {
        return;
    }

    m_isPressed = pressed;
    applyStateColor();
}

int StatefulPushButton::radius() const { return m_radius; }
void StatefulPushButton::setRadius(int r) { m_radius = r; update(); }

QColor StatefulPushButton::normalColor() const { return m_normalColor; }
void StatefulPushButton::setNormalColor(const QColor& c) { m_normalColor = c; applyStateColor(); }

QColor StatefulPushButton::hoverColor() const { return m_hoverColor; }
void StatefulPushButton::setHoverColor(const QColor& c) { m_hoverColor = c; }

QColor StatefulPushButton::pressColor() const { return m_pressColor; }
void StatefulPushButton::setPressColor(const QColor& c) { m_pressColor = c; }

QColor StatefulPushButton::disabledColor() const { return m_disabledColor; }
void StatefulPushButton::setDisabledColor(const QColor& c) { m_disabledColor = c; }

QColor StatefulPushButton::currentColor() const { return m_currentColor; }
void StatefulPushButton::setCurrentColor(const QColor& c)
{
    m_currentColor = c;
    update();
}

QColor StatefulPushButton::textColor() const { return m_textColor; }
void StatefulPushButton::setTextColor(const QColor& c) { m_textColor = c; update(); }

QColor StatefulPushButton::borderColor() const { return m_borderColor; }
void StatefulPushButton::setBorderColor(const QColor& c) { m_borderColor = c; update(); }

int StatefulPushButton::borderWidth() const { return m_borderWidth; }
void StatefulPushButton::setBorderWidth(int w) { m_borderWidth = w; update(); }

bool StatefulPushButton::isFlat() const { return m_isFlat; }
void StatefulPushButton::setFlat(bool f) { m_isFlat = f; update(); }

void StatefulPushButton::setAnimationDuration(int ms)
{
    m_animationDuration = ms;
    m_colorAnimation->setDuration(ms);
}

int StatefulPushButton::animationDuration() const { return m_animationDuration; }

void StatefulPushButton::setDefaultStyle() {
    setNormalColor(0xE0E0E0); setHoverColor(0xD0D0D0);
    setPressColor(0xC0C0C0); setTextColor(0x333333);
}

void StatefulPushButton::setPrimaryStyle() {
    setNormalColor(0x0099FF); setHoverColor(0x0088EE);
    setPressColor(0x0077DD); setTextColor(0xFFFFFF);
}

void StatefulPushButton::setSuccessStyle() {
    setNormalColor(0x28A745); setHoverColor(0x218838);
    setPressColor(0x1E7E34); setTextColor(0xFFFFFF);
}

void StatefulPushButton::setWarningStyle() {
    setNormalColor(0xFFC107); setHoverColor(0xE0A800);
    setPressColor(0xD39E00); setTextColor(0x333333);
}

void StatefulPushButton::setDangerStyle() {
    setNormalColor(0xDC3545); setHoverColor(0xC82333);
    setPressColor(0xBD2130); setTextColor(0xFFFFFF);
}

void StatefulPushButton::enterEvent(QEnterEvent* e)
{
    if (!isEnabled()) return;
    updateHoverState(true);
    QPushButton::enterEvent(e);
}

void StatefulPushButton::leaveEvent(QEvent* e)
{
    if (!isEnabled()) return;
    updateHoverState(false);
    updatePressState(false);
    QPushButton::leaveEvent(e);
}

void StatefulPushButton::mouseMoveEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    updateHoverState(rect().contains(e->position().toPoint()));
    QPushButton::mouseMoveEvent(e);
}

void StatefulPushButton::mousePressEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    if (e->button() == Qt::LeftButton) {
        updateHoverState(rect().contains(e->position().toPoint()));
        updatePressState(rect().contains(e->position().toPoint()));
    }
    QPushButton::mousePressEvent(e);
}

void StatefulPushButton::mouseReleaseEvent(QMouseEvent* e)
{
    if (!isEnabled()) return;
    if (e->button() == Qt::LeftButton) {
        updateHoverState(rect().contains(e->position().toPoint()));
        updatePressState(false);
    }
    QPushButton::mouseReleaseEvent(e);
}

void StatefulPushButton::paintEvent(QPaintEvent* e)
{
    Q_UNUSED(e);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addRoundedRect(rect(), m_radius, m_radius);

    if (!m_isFlat)
        p.fillPath(path, m_currentColor);

    if (m_borderWidth > 0) {
        p.setPen(QPen(m_borderColor, m_borderWidth));
        p.drawPath(path);
    }

    p.setPen(m_textColor);
    const QString buttonText = text();
    const QFontMetrics metrics(font());
    const QRect textBounds = metrics.tightBoundingRect(buttonText);
    const QPoint textOrigin((width() - textBounds.width()) / 2 - textBounds.left(),
                            (height() - textBounds.height()) / 2 - textBounds.top());
    p.drawText(textOrigin, buttonText);
}

bool StatefulPushButton::event(QEvent* e)
{
    if (e->type() == QEvent::HoverEnter) {
        updateHoverState(true);
    } else if (e->type() == QEvent::HoverMove) {
        if (const auto* hoverEvent = static_cast<QHoverEvent*>(e)) {
            updateHoverState(rect().contains(hoverEvent->position().toPoint()));
        }
    } else if (e->type() == QEvent::HoverLeave) {
        updateHoverState(false);
        updatePressState(false);
    } else if (e->type() == QEvent::EnabledChange
        || e->type() == QEvent::Hide
        || e->type() == QEvent::WindowDeactivate) {
        m_isHovered = false;
        m_isPressed = false;
        applyStateColor();
    }
    return QPushButton::event(e);
}
