#include "RedstoneLampSwitch.h"

#include "shared/services/ImageService.h"

#include <QPainter>
#include <QPaintEvent>
#include <QPropertyAnimation>

namespace {
constexpr int kSwitchWidth = 46;
constexpr int kSwitchHeight = 24;
constexpr int kTrackInset = 2;
constexpr qreal kTrackRadius = 2.0;
constexpr qreal kTrackBorderWidth = 2.0;
constexpr int kLampMargin = 3;
constexpr int kLampSize = 18;
constexpr int kAnimationDuration = 320;

const QColor kOffBackground(0xAA, 0xAA, 0xAA);
const QColor kOnBackground(0xFBF3E4);
} // namespace

RedstoneLampSwitch::RedstoneLampSwitch(QWidget* parent)
    : QPushButton(parent)
    , m_backgroundColor(kOffBackground)
    , m_lampIconSource(QStringLiteral(":/resources/icon/redstone_lamp.png"))
    , m_litLampIconSource(QStringLiteral(":/resources/icon/redstone_lamp_on.png"))
    , m_chargeAnimation(new QPropertyAnimation(this, "redstoneCharge", this))
    , m_glowAnimation(new QPropertyAnimation(this, "lampGlowOpacity", this))
    , m_backgroundAnimation(new QPropertyAnimation(this, "backgroundColor", this))
{
    setCheckable(false);
    setCursor(Qt::PointingHandCursor);
    setFocusPolicy(Qt::NoFocus);
    setFlat(true);
    setFixedSize(kSwitchWidth, kSwitchHeight);
    setAttribute(Qt::WA_Hover);
    setAttribute(Qt::WA_MacShowFocusRect, false);

    for (QPropertyAnimation* animation : {m_chargeAnimation, m_glowAnimation, m_backgroundAnimation}) {
        animation->setDuration(kAnimationDuration);
        animation->setEasingCurve(QEasingCurve::OutCubic);
    }

    connect(this, &QPushButton::clicked, this, [this]() {
        toggleLampTarget();
    });
    connect(m_chargeAnimation, &QPropertyAnimation::finished, this, [this]() {
        commitLampTarget();
    });
}

QSize RedstoneLampSwitch::sizeHint() const
{
    return QSize(kSwitchWidth, kSwitchHeight);
}

QSize RedstoneLampSwitch::minimumSizeHint() const
{
    return sizeHint();
}

qreal RedstoneLampSwitch::redstoneCharge() const
{
    return m_redstoneCharge;
}

void RedstoneLampSwitch::setRedstoneCharge(qreal charge)
{
    const qreal boundedCharge = qBound(0.0, charge, 1.0);
    if (qFuzzyCompare(m_redstoneCharge, boundedCharge)) {
        return;
    }

    m_redstoneCharge = boundedCharge;
    update();
}

qreal RedstoneLampSwitch::lampGlowOpacity() const
{
    return m_lampGlowOpacity;
}

void RedstoneLampSwitch::setLampGlowOpacity(qreal opacity)
{
    const qreal boundedOpacity = qBound(0.0, opacity, 1.0);
    if (qFuzzyCompare(m_lampGlowOpacity, boundedOpacity)) {
        return;
    }

    m_lampGlowOpacity = boundedOpacity;
    update();
}

QColor RedstoneLampSwitch::backgroundColor() const
{
    return m_backgroundColor;
}

void RedstoneLampSwitch::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor == color) {
        return;
    }

    m_backgroundColor = color;
    update();
}

void RedstoneLampSwitch::setLampChecked(bool checked, bool animated)
{
    if (m_lampChecked == checked && m_targetLampChecked == checked) {
        return;
    }

    m_lampChecked = checked;
    m_targetLampChecked = checked;
    updateLampState(animated);
}

void RedstoneLampSwitch::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    const QRectF trackRect = QRectF(rect()).adjusted(kTrackInset,
                                                     kTrackInset,
                                                     -kTrackInset,
                                                     -kTrackInset);
    painter.setPen(Qt::NoPen);
    painter.setBrush(m_backgroundColor);
    const QRectF borderRect = trackRect.adjusted(kTrackBorderWidth / 2.0,
                                                 kTrackBorderWidth / 2.0,
                                                 -kTrackBorderWidth / 2.0,
                                                 -kTrackBorderWidth / 2.0);
    painter.drawRoundedRect(borderRect, kTrackRadius, kTrackRadius);

    if (underMouse() && isEnabled()) {
        painter.setPen(QPen(QColor(0, 0, 0, 36), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(borderRect, kTrackRadius, kTrackRadius);
    }

    painter.setPen(QPen(Qt::black, kTrackBorderWidth));
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(borderRect, kTrackRadius, kTrackRadius);

    const int iconSize = qMin(kLampSize, height() - 2 * kLampMargin);
    const qreal leftX = kLampMargin;
    const qreal rightX = width() - kLampMargin - iconSize;
    const qreal iconX = leftX + (rightX - leftX) * m_redstoneCharge;
    const QRectF iconRect(iconX,
                          (height() - iconSize) / 2.0,
                          iconSize,
                          iconSize);
    const QSize iconTargetSize(iconSize, iconSize);
    const qreal dpr = devicePixelRatioF();
    const QPixmap lampIcon = ImageService::instance().scaled(m_lampIconSource,
                                                             iconTargetSize,
                                                             Qt::KeepAspectRatio,
                                                             dpr);
    const QPixmap litLampIcon = ImageService::instance().scaled(m_litLampIconSource,
                                                                iconTargetSize,
                                                                Qt::KeepAspectRatio,
                                                                dpr);

    painter.drawPixmap(iconRect.toAlignedRect(), lampIcon);
    painter.setOpacity(m_lampGlowOpacity);
    painter.drawPixmap(iconRect.toAlignedRect(), litLampIcon);
}

void RedstoneLampSwitch::updateLampState(bool animated)
{
    const bool active = m_targetLampChecked;
    const qreal targetCharge = active ? 1.0 : 0.0;
    const qreal targetGlow = active ? 1.0 : 0.0;
    const QColor targetBackground = active ? kOnBackground : kOffBackground;

    stopAnimations();

    if (!animated) {
        setRedstoneCharge(targetCharge);
        setLampGlowOpacity(targetGlow);
        setBackgroundColor(targetBackground);
        return;
    }

    m_chargeAnimation->setStartValue(m_redstoneCharge);
    m_chargeAnimation->setEndValue(targetCharge);
    m_glowAnimation->setStartValue(m_lampGlowOpacity);
    m_glowAnimation->setEndValue(targetGlow);
    m_backgroundAnimation->setStartValue(m_backgroundColor);
    m_backgroundAnimation->setEndValue(targetBackground);

    m_chargeAnimation->start();
    m_glowAnimation->start();
    m_backgroundAnimation->start();
}

void RedstoneLampSwitch::stopAnimations()
{
    m_chargeAnimation->stop();
    m_glowAnimation->stop();
    m_backgroundAnimation->stop();
}

void RedstoneLampSwitch::toggleLampTarget()
{
    if (!isEnabled()) {
        return;
    }

    m_targetLampChecked = !m_targetLampChecked;
    updateLampState(true);
}

void RedstoneLampSwitch::commitLampTarget()
{
    if (m_lampChecked == m_targetLampChecked) {
        return;
    }

    m_lampChecked = m_targetLampChecked;
    emit redstonePowerChanged(m_lampChecked);
}
