#pragma once

#include <QColor>
#include <QWidget>

class QKeyEvent;
class QLineEdit;
class QMouseEvent;
class QPaintEvent;
class QResizeEvent;

class MinecraftButton;

class ThemeColorPalette final : public QWidget
{
    Q_OBJECT

public:
    explicit ThemeColorPalette(QWidget* parent = nullptr);

    QColor currentColor() const;
    void setCurrentColor(const QColor& color);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void accepted(const QColor& color);
    void canceled();

protected:
    bool event(QEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void layoutControls();
    void rebuildSpectrumCache();
    void setColorFromHsv(qreal hue, qreal saturation, qreal value, bool updateInput);
    void setColorFromRgb(const QColor& color, bool updateInput);
    void setColorFromSpectrumPosition(const QPoint& point);
    void setBrightnessFromPosition(int x);
    void updateInputText();
    bool applyInputText();
    void acceptSelection();
    void cancelSelection();

    QRect spectrumRect() const;
    QRect sunRect() const;
    QRect moonRect() const;
    QRect sliderTrackRect() const;
    QRect sliderHandleRect() const;
    QRect sampleRect() const;
    QPoint spectrumHandleCenter() const;
    QString normalizedColorText() const;

    QLineEdit* m_input = nullptr;
    MinecraftButton* m_confirmButton = nullptr;
    MinecraftButton* m_cancelButton = nullptr;

    mutable QRect m_spectrumRect;
    mutable QRect m_sunRect;
    mutable QRect m_moonRect;
    mutable QRect m_sliderTrackRect;
    mutable QRect m_sampleRect;

    QImage m_spectrumCache;
    QSize m_cachedSpectrumSize;
    qreal m_cachedValue = -1.0;

    QColor m_color = QColor(0x00, 0x99, 0xff);
    qreal m_hue = 0.58;
    qreal m_saturation = 1.0;
    qreal m_value = 1.0;
    bool m_draggingSpectrum = false;
    bool m_draggingBrightness = false;
    bool m_sliderHandleHovered = false;
};
