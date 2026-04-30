#pragma once

#include <QColor>
#include <QWidget>

class QLineEdit;
class PaintedLabel;

class InlineEditableText : public QWidget
{
    Q_OBJECT

public:
    explicit InlineEditableText(QWidget* parent = nullptr);
    ~InlineEditableText() override;

    QString text() const;
    void setText(const QString& text);

    QString placeholderText() const;
    void setPlaceholderText(const QString& text);

    Qt::Alignment alignment() const;
    void setAlignment(Qt::Alignment alignment);

    void setTextColor(const QColor& color);
    void setPlaceholderTextColor(const QColor& color);
    void setNormalBackgroundColor(const QColor& color);
    void setHoverBackgroundColor(const QColor& color);
    void setFocusBackgroundColor(const QColor& color);
    void setNormalBorderColor(const QColor& color);
    void setFocusBorderColor(const QColor& color);
    void setBorderWidth(int width);
    void setRadius(int radius);
    void setHorizontalPadding(int padding);

    bool isEditing() const;
    void startEditing();
    void finishEditing();

signals:
    void editingFinished();

protected:
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    bool event(QEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void updateChildGeometry();
    void updateLabelText();
    void updatePalettes();
    QColor currentBackgroundColor() const;
    QColor currentBorderColor() const;

    PaintedLabel* m_label = nullptr;
    QLineEdit* m_edit = nullptr;
    QString m_text;
    QString m_placeholderText;
    Qt::Alignment m_alignment = Qt::AlignRight | Qt::AlignVCenter;
    QColor m_textColor;
    QColor m_placeholderTextColor;
    QColor m_normalBackgroundColor;
    QColor m_hoverBackgroundColor;
    QColor m_focusBackgroundColor;
    QColor m_normalBorderColor;
    QColor m_focusBorderColor;
    int m_borderWidth = 0;
    int m_radius = 0;
    int m_horizontalPadding = 0;
    bool m_hovered = false;
    bool m_committing = false;
    bool m_updatingPalette = false;
};
