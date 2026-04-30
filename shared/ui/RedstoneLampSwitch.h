#pragma once

#include <QColor>
#include <QPushButton>
#include <QString>

class QPropertyAnimation;

class RedstoneLampSwitch : public QPushButton
{
    Q_OBJECT
    Q_PROPERTY(qreal redstoneCharge READ redstoneCharge WRITE setRedstoneCharge)
    Q_PROPERTY(qreal lampGlowOpacity READ lampGlowOpacity WRITE setLampGlowOpacity)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor WRITE setBackgroundColor)

public:
    explicit RedstoneLampSwitch(QWidget* parent = nullptr);

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

    qreal redstoneCharge() const;
    void setRedstoneCharge(qreal charge);

    qreal lampGlowOpacity() const;
    void setLampGlowOpacity(qreal opacity);

    QColor backgroundColor() const;
    void setBackgroundColor(const QColor& color);

    void setLampChecked(bool checked, bool animated = false);

signals:
    void redstonePowerChanged(bool powered);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void updateLampState(bool animated);
    void stopAnimations();
    void toggleLampTarget();
    void commitLampTarget();

    bool m_lampChecked = false;
    bool m_targetLampChecked = false;
    qreal m_redstoneCharge = 0.0;
    qreal m_lampGlowOpacity = 0.0;
    QColor m_backgroundColor;
    QString m_lampIconSource;
    QString m_litLampIconSource;
    QPropertyAnimation* m_chargeAnimation = nullptr;
    QPropertyAnimation* m_glowAnimation = nullptr;
    QPropertyAnimation* m_backgroundAnimation = nullptr;
};
