#pragma once

#include <QMargins>
#include <QPushButton>

class MinecraftButton final : public QPushButton
{
    Q_OBJECT

public:
    explicit MinecraftButton(QWidget* parent = nullptr);
    explicit MinecraftButton(const QString& text, QWidget* parent = nullptr);

    void setNormalImage(const QString& source);
    void setHoverImage(const QString& source);
    void setDisabledImage(const QString& source);
    void setSourceMargins(const QMargins& margins);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;

private:
    void initialize();
    void setHovered(bool hovered);
    QString currentImage() const;
    void drawNinePatch(QPainter& painter, const QPixmap& pixmap, const QRect& targetRect) const;

    QString m_normalImage;
    QString m_hoverImage;
    QString m_disabledImage;
    QMargins m_sourceMargins;
    bool m_hovered = false;
};
