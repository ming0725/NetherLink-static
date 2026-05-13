#pragma once

#include <QColor>
#include <QLabel>

class PaintedLabel : public QLabel
{
    Q_OBJECT

public:
    explicit PaintedLabel(QWidget* parent = nullptr);
    explicit PaintedLabel(const QString& text, QWidget* parent = nullptr);

    QColor textColor() const;
    void setTextColor(const QColor& color);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QColor m_textColor;
};
