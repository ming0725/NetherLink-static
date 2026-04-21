#pragma once

#include <QWidget>

class PostOverlay : public QWidget
{
public:
    explicit PostOverlay(QWidget* parent = nullptr);

    void setOverlayOpacity(qreal opacity);
    qreal overlayOpacity() const { return m_opacity; }

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    qreal m_opacity = 0.0;
};
