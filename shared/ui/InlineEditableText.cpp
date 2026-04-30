#include "InlineEditableText.h"

#include "PaintedLabel.h"

#include <QApplication>
#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QResizeEvent>

InlineEditableText::InlineEditableText(QWidget* parent)
    : QWidget(parent)
    , m_label(new PaintedLabel(this))
    , m_edit(new QLineEdit(this))
    , m_textColor(0x55, 0x55, 0x55)
    , m_placeholderTextColor(0x88, 0x88, 0x88)
    , m_normalBackgroundColor(Qt::transparent)
    , m_hoverBackgroundColor(Qt::transparent)
    , m_focusBackgroundColor(Qt::transparent)
    , m_normalBorderColor(Qt::transparent)
    , m_focusBorderColor(Qt::transparent)
{
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::ClickFocus);
    setAutoFillBackground(false);

    m_label->setAlignment(m_alignment);
    m_label->installEventFilter(this);
    m_label->setAutoFillBackground(false);

    m_edit->setFrame(false);
    m_edit->setAutoFillBackground(false);
    m_edit->setAttribute(Qt::WA_MacShowFocusRect, false);
    m_edit->setAlignment(m_alignment);
    m_edit->installEventFilter(this);
    m_edit->hide();

    connect(m_edit, &QLineEdit::editingFinished, this, &InlineEditableText::finishEditing);

    if (qApp) {
        qApp->installEventFilter(this);
    }

    updatePalettes();
    updateLabelText();
}

InlineEditableText::~InlineEditableText()
{
    if (qApp) {
        qApp->removeEventFilter(this);
    }
}

QString InlineEditableText::text() const
{
    return isEditing() ? m_edit->text() : m_text;
}

void InlineEditableText::setText(const QString& text)
{
    const QString nextText = text.trimmed();
    if (m_text == nextText && !isEditing()) {
        return;
    }

    m_text = nextText;
    if (!isEditing()) {
        m_edit->setText(m_text);
        updateLabelText();
    }
}

QString InlineEditableText::placeholderText() const
{
    return m_placeholderText;
}

void InlineEditableText::setPlaceholderText(const QString& text)
{
    m_placeholderText = text;
    m_edit->setPlaceholderText(text);
    updateLabelText();
}

Qt::Alignment InlineEditableText::alignment() const
{
    return m_alignment;
}

void InlineEditableText::setAlignment(Qt::Alignment alignment)
{
    m_alignment = alignment;
    if (!(m_alignment & Qt::AlignVertical_Mask)) {
        m_alignment |= Qt::AlignVCenter;
    }
    m_label->setAlignment(m_alignment);
    m_edit->setAlignment(m_alignment);
    update();
}

void InlineEditableText::setTextColor(const QColor& color)
{
    m_textColor = color;
    updatePalettes();
}

void InlineEditableText::setPlaceholderTextColor(const QColor& color)
{
    m_placeholderTextColor = color;
    updatePalettes();
}

void InlineEditableText::setNormalBackgroundColor(const QColor& color)
{
    m_normalBackgroundColor = color;
    update();
}

void InlineEditableText::setHoverBackgroundColor(const QColor& color)
{
    m_hoverBackgroundColor = color;
    update();
}

void InlineEditableText::setFocusBackgroundColor(const QColor& color)
{
    m_focusBackgroundColor = color;
    update();
}

void InlineEditableText::setNormalBorderColor(const QColor& color)
{
    m_normalBorderColor = color;
    update();
}

void InlineEditableText::setFocusBorderColor(const QColor& color)
{
    m_focusBorderColor = color;
    update();
}

void InlineEditableText::setBorderWidth(int width)
{
    m_borderWidth = qMax(0, width);
    update();
}

void InlineEditableText::setRadius(int radius)
{
    m_radius = qMax(0, radius);
    update();
}

void InlineEditableText::setHorizontalPadding(int padding)
{
    m_horizontalPadding = qMax(0, padding);
    updateChildGeometry();
    update();
}

bool InlineEditableText::isEditing() const
{
    return m_edit->isVisible();
}

void InlineEditableText::startEditing()
{
    if (isEditing()) {
        return;
    }

    m_label->hide();
    m_edit->setText(m_text);
    m_edit->show();
    m_edit->setFocus(Qt::MouseFocusReason);
    m_edit->selectAll();
    update();
}

void InlineEditableText::finishEditing()
{
    if (m_committing || !isEditing()) {
        return;
    }

    m_committing = true;
    m_text = m_edit->text().trimmed();
    m_edit->setText(m_text);
    m_edit->hide();
    m_label->show();
    updateLabelText();
    update();
    emit editingFinished();
    m_committing = false;
}

void InlineEditableText::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateChildGeometry();
}

void InlineEditableText::mousePressEvent(QMouseEvent* event)
{
    startEditing();
    QWidget::mousePressEvent(event);
}

void InlineEditableText::enterEvent(QEnterEvent* event)
{
    m_hovered = true;
    update();
    QWidget::enterEvent(event);
}

void InlineEditableText::leaveEvent(QEvent* event)
{
    m_hovered = false;
    update();
    QWidget::leaveEvent(event);
}

void InlineEditableText::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);

    const QColor background = currentBackgroundColor();
    const QColor border = currentBorderColor();
    const QRectF paintRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);

    if (background.alpha() > 0) {
        painter.setPen(Qt::NoPen);
        painter.setBrush(background);
        painter.drawRoundedRect(paintRect, m_radius, m_radius);
    }

    if (m_borderWidth > 0 && border.alpha() > 0) {
        painter.setPen(QPen(border, m_borderWidth));
        painter.setBrush(Qt::NoBrush);
        const qreal inset = m_borderWidth / 2.0;
        painter.drawRoundedRect(QRectF(rect()).adjusted(inset, inset, -inset, -inset),
                                m_radius,
                                m_radius);
    }
}

bool InlineEditableText::event(QEvent* event)
{
    if (event->type() == QEvent::FontChange) {
        m_label->setFont(font());
        m_edit->setFont(font());
    } else if (event->type() == QEvent::PaletteChange && !m_updatingPalette) {
        updatePalettes();
        updateLabelText();
    }
    return QWidget::event(event);
}

bool InlineEditableText::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_label && event->type() == QEvent::MouseButtonPress) {
        startEditing();
        return true;
    }

    if ((watched == m_label || watched == m_edit) &&
        event->type() == QEvent::PaletteChange &&
        !m_updatingPalette) {
        updatePalettes();
        updateLabelText();
    }

    if (isEditing() && event->type() == QEvent::MouseButtonPress) {
        const auto* mouseEvent = static_cast<QMouseEvent*>(event);
        const QPoint localPos = mapFromGlobal(mouseEvent->globalPosition().toPoint());
        if (!rect().contains(localPos)) {
            finishEditing();
            m_edit->clearFocus();
        }
    }

    return QWidget::eventFilter(watched, event);
}

void InlineEditableText::updateChildGeometry()
{
    const QRect contentRect = rect().adjusted(m_horizontalPadding, 0, -m_horizontalPadding, 0);
    m_label->setGeometry(contentRect);
    m_edit->setGeometry(contentRect);
}

void InlineEditableText::updateLabelText()
{
    m_label->setText(m_text.isEmpty() ? m_placeholderText : m_text);
    m_label->setTextColor(m_text.isEmpty() ? m_placeholderTextColor : m_textColor);
}

void InlineEditableText::updatePalettes()
{
    m_updatingPalette = true;

    m_label->setTextColor(m_text.isEmpty() ? m_placeholderTextColor : m_textColor);

    QPalette editPalette = m_edit->palette();
    editPalette.setColor(QPalette::Base, Qt::transparent);
    editPalette.setColor(QPalette::Text, m_textColor);
    editPalette.setColor(QPalette::PlaceholderText, m_placeholderTextColor);
    m_edit->setPalette(editPalette);

    m_updatingPalette = false;
}

QColor InlineEditableText::currentBackgroundColor() const
{
    if (isEditing()) {
        return m_focusBackgroundColor;
    }
    return m_hovered ? m_hoverBackgroundColor : m_normalBackgroundColor;
}

QColor InlineEditableText::currentBorderColor() const
{
    return isEditing() ? m_focusBorderColor : m_normalBorderColor;
}
