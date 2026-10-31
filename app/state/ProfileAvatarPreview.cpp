#include "app/state/ProfileAvatarPreview.h"

#include "app/state/CurrentUserPopupStyle.h"
#include "shared/services/AudioService.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"

#include <QDir>
#include <QEasingCurve>
#include <QEvent>
#include <QFileDialog>
#include <QImageReader>
#include <QMouseEvent>
#include <QPainter>
#include <QStandardPaths>
#include <QVariantAnimation>

using namespace CurrentUserPopupStyle;

ProfileAvatarPreview::ProfileAvatarPreview(QWidget* parent)
    : QWidget(parent)
    , m_hoverAnimation(new QVariantAnimation(this))
{
    setFixedSize(kProfileEditAvatarSize, kProfileEditAvatarSize);
    setCursor(Qt::PointingHandCursor);
    setMouseTracking(true);
    m_hoverAnimation->setDuration(160);
    m_hoverAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_hoverAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_hoverProgress = value.toReal();
        update();
    });
}

void ProfileAvatarPreview::setAvatarSource(const QString& source)
{
    if (m_avatarSource == source) {
        return;
    }
    m_avatarSource = source;
    update();
}

bool ProfileAvatarPreview::event(QEvent* event)
{
    if (event->type() == QEvent::Enter) {
        animateHover(1.0);
    } else if (event->type() == QEvent::Leave) {
        animateHover(0.0);
    }
    return QWidget::event(event);
}

void ProfileAvatarPreview::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing | QPainter::SmoothPixmapTransform, true);
    const QRect avatarRect = rect();
    const QPixmap avatar = ImageService::instance().circularAvatar(m_avatarSource,
                                                                   kProfileEditAvatarSize,
                                                                   devicePixelRatioF());
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 0, 0));
    painter.drawEllipse(avatarRect);

    if (!avatar.isNull()) {
        painter.save();
        painter.setOpacity(1.0 - m_hoverProgress * 0.48);
        painter.drawPixmap(avatarRect, avatar);
        painter.restore();
    } else {
        MediaPlaceholderRenderer::drawAvatar(&painter, avatarRect);
    }

    if (m_hoverProgress > 0.01) {
        painter.save();
        painter.setOpacity(m_hoverProgress);
        const int iconSize = 30;
        const QRect iconRect(avatarRect.center().x() - iconSize / 2,
                             avatarRect.center().y() - iconSize / 2,
                             iconSize,
                             iconSize);
        const QPixmap icon = ImageService::instance().scaled(QStringLiteral(":/resources/icon/painting.png"),
                                                             iconRect.size(),
                                                             Qt::KeepAspectRatio,
                                                             devicePixelRatioF());
        if (!icon.isNull()) {
            painter.drawPixmap(iconRect, icon);
        }
        painter.restore();
    }
}

void ProfileAvatarPreview::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && rect().contains(event->pos())) {
        AudioService::instance().playButtonClick();
        QString defaultPath = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
        if (defaultPath.isEmpty()) {
            defaultPath = QDir::homePath();
        }
        const QString filePath = QFileDialog::getOpenFileName(
                this,
                QStringLiteral("选择头像"),
                defaultPath,
                QStringLiteral("图片文件 (*.png *.jpg *.jpeg *.bmp *.webp);;所有文件 (*)"));
        if (!filePath.isEmpty()) {
            QImageReader reader(filePath);
            reader.setAutoTransform(true);
            const QImage image = reader.read();
            if (!image.isNull() && avatarImageSelected) {
                avatarImageSelected(image);
            }
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ProfileAvatarPreview::animateHover(qreal target)
{
    if (qFuzzyCompare(m_hoverProgress, target)) {
        return;
    }
    m_hoverAnimation->stop();
    m_hoverAnimation->setStartValue(m_hoverProgress);
    m_hoverAnimation->setEndValue(target);
    m_hoverAnimation->start();
}
