#include "LoginWindow.h"

#include "LoginAccountRepository.h"
#include "LoginInputField.h"
#include "RegisterWindow.h"
#include "app/state/CurrentUser.h"
#include "app/state/CurrentUserProfileRepository.h"
#ifndef Q_OS_MACOS
#include "platform/windows/WindowsWindowControlButton.h"
#endif
#include "shared/services/AppFonts.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/effects/FastGaussianBlur.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/StatefulPushButton.h"

#include <QAbstractButton>
#include <QApplication>
#include <QCloseEvent>
#include <QFutureWatcher>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRandomGenerator>
#include <QScreen>
#include <QTimer>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <functional>
#include <utility>

namespace {
constexpr int kWindowWidth = 420;
constexpr int kWindowHeight = 620;
constexpr int kTitleBarHeight = 38;
constexpr int kFormWidth = 294;
constexpr int kSocialButtonSize = 44;
constexpr int kSocialButtonGeometrySize = 52;
constexpr int kSocialIconSize = 22;
constexpr int kSocialButtonGap = 18;
constexpr int kSocialButtonHitInset = (kSocialButtonGeometrySize - kSocialButtonSize) / 2;
constexpr int kSocialButtonAreaWidth = kSocialButtonSize * 3 + kSocialButtonGap * 2;
constexpr int kSocialBottomSpacing = 12 - (kSocialButtonGeometrySize - kSocialButtonSize);
constexpr int kBackgroundFrameMs = 16;
constexpr int kBackgroundLayerRefreshMs = 16;
constexpr int kShowMainWindowDelayMs = 1600;
constexpr int kAccountPopupGap = 5;
constexpr int kAccountPopupRowHeight = 52;
constexpr int kAccountPopupRadius = 10;
constexpr int kAccountPopupAvatarSize = 36;
constexpr int kAccountPopupMaxRows = 4;
constexpr int kAccountPopupDeleteSize = 22;
constexpr qreal kBackgroundBlurRenderScale = 0.46;
constexpr qreal kBackgroundBlurRadius = 38.0;

struct BackgroundLightFrame {
    QColor color;
    QPointF position;
    qreal radius = 0.0;
    qreal opacity = 0.0;
    qreal rotation = 0.0;
};

qreal randomRange(qreal minimum, qreal maximum)
{
    return minimum + (maximum - minimum) * QRandomGenerator::global()->generateDouble();
}

qreal easeInOut(qreal t)
{
    t = qBound<qreal>(0.0, t, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

QColor withAlphaF(QColor color, qreal alpha)
{
    color.setAlphaF(qBound<qreal>(0.0, alpha, 1.0));
    return color;
}

QColor loginAvatarStrokeColor()
{
    QColor color = ThemeManager::instance().color(ThemeColor::TertiaryText);
    color.setAlpha(190);
    return color;
}

QString loginAccountDisplayText(const LoginAccount& account)
{
    if (account.displayName.isEmpty() || account.displayName == account.accountId) {
        return account.accountId;
    }
    return QStringLiteral("%1 (%2)").arg(account.displayName, account.accountId);
}

qreal wrapHue(qreal hue)
{
    while (hue < 0.0) {
        hue += 1.0;
    }
    while (hue >= 1.0) {
        hue -= 1.0;
    }
    return hue;
}

qreal hueOffset(qreal sourceHue, qreal targetHue)
{
    qreal offset = targetHue - sourceHue;
    if (offset > 0.5) {
        offset -= 1.0;
    } else if (offset < -0.5) {
        offset += 1.0;
    }
    return offset;
}

QColor loginBackgroundColorFromTheme(const QColor& referenceColor)
{
    const QColor referenceThemeColor(0x00, 0x99, 0xff);
    QColor themeColor = ThemeManager::instance().themeColor().toRgb();
    if (!themeColor.isValid()) {
        themeColor = referenceThemeColor;
    }
    themeColor.setAlpha(255);

    if (themeColor == referenceThemeColor) {
        return referenceColor;
    }
    if (referenceColor == referenceThemeColor) {
        return themeColor;
    }

    float themeHue = 0.0f;
    float themeSaturation = 0.0f;
    float themeValue = 0.0f;
    float alpha = 1.0f;
    themeColor.toHsv().getHsvF(&themeHue, &themeSaturation, &themeValue, &alpha);

    float referenceThemeHue = 0.0f;
    float referenceThemeSaturation = 0.0f;
    float referenceThemeValue = 0.0f;
    referenceThemeColor.toHsv().getHsvF(&referenceThemeHue,
                                        &referenceThemeSaturation,
                                        &referenceThemeValue,
                                        nullptr);

    float referenceHue = 0.0f;
    float referenceSaturation = 0.0f;
    float referenceValue = 0.0f;
    referenceColor.toHsv().getHsvF(&referenceHue, &referenceSaturation, &referenceValue, nullptr);

    if (themeHue < 0.0) {
        themeHue = referenceThemeHue;
    }

    const qreal saturationScale = referenceThemeSaturation > 0.0
            ? referenceSaturation / referenceThemeSaturation
            : 1.0;
    const qreal valueScale = referenceThemeValue > 0.0
            ? referenceValue / referenceThemeValue
            : 1.0;

    return QColor::fromHsvF(wrapHue(themeHue + hueOffset(referenceThemeHue, referenceHue)),
                            qBound<qreal>(0.0, themeSaturation * saturationScale, 1.0),
                            qBound<qreal>(0.0, themeValue * valueScale, 1.0),
                            alpha).toRgb();
}

QVector<QColor> loginBackgroundColors()
{
    return {
        loginBackgroundColorFromTheme(QColor(0x00, 0x99, 0xff)),
        loginBackgroundColorFromTheme(QColor(0x00, 0xd6, 0xc9)),
        loginBackgroundColorFromTheme(QColor(0x7a, 0x6c, 0xff)),
    };
}

QLinearGradient loginBackgroundWash(const QRectF& bounds, bool darkMode)
{
    QLinearGradient wash(bounds.topLeft(), bounds.bottomRight());
    if (darkMode) {
        wash.setColorAt(0.0, QColor(0x72, 0xa9, 0xd8));
        wash.setColorAt(0.48, QColor(0x8b, 0xb5, 0xde));
        wash.setColorAt(1.0, QColor(0x88, 0xca, 0xd7));
    } else {
        wash.setColorAt(0.0, QColor(0xe8, 0xf6, 0xff));
        wash.setColorAt(0.50, QColor(0xe9, 0xfb, 0xff));
        wash.setColorAt(1.0, QColor(0xf0, 0xee, 0xff));
    }
    return wash;
}

QImage renderBlurredLoginBackground(QSize logicalSize,
                                    qreal devicePixelRatio,
                                    qreal renderScale,
                                    qreal blurRadius,
                                    QVector<BackgroundLightFrame> lights,
                                    bool darkMode)
{
    if (!logicalSize.isValid()) {
        return {};
    }

    const qreal layerDevicePixelRatio = qMax<qreal>(0.1, devicePixelRatio * renderScale);
    const QSize pixelSize(qMax(1, qRound(logicalSize.width() * layerDevicePixelRatio)),
                          qMax(1, qRound(logicalSize.height() * layerDevicePixelRatio)));
    QImage source(pixelSize, QImage::Format_ARGB32_Premultiplied);
    source.setDevicePixelRatio(layerDevicePixelRatio);
    source.fill(Qt::transparent);

    QPainter painter(&source);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const QRectF bounds(QPointF(0.0, 0.0), QSizeF(logicalSize));
    painter.fillRect(bounds, loginBackgroundWash(bounds, darkMode));

    for (const BackgroundLightFrame& light : lights) {
        const QColor core = withAlphaF(light.color, light.opacity);
        const QColor soft = withAlphaF(light.color, light.opacity * 0.62);
        painter.setPen(Qt::NoPen);
        painter.setBrush(core);
        painter.drawEllipse(light.position, light.radius * 1.18, light.radius * 0.88);

        painter.save();
        painter.translate(light.position);
        painter.rotate(light.rotation);
        painter.setBrush(soft);
        painter.drawRoundedRect(QRectF(-light.radius * 0.72,
                                       -light.radius * 0.20,
                                       light.radius * 1.44,
                                       light.radius * 0.40),
                                light.radius * 0.20,
                                light.radius * 0.20);

        QPainterPath shard;
        shard.moveTo(QPointF(-light.radius * 0.54, light.radius * 0.12));
        shard.lineTo(QPointF(-light.radius * 0.08, -light.radius * 0.50));
        shard.lineTo(QPointF(light.radius * 0.62, -light.radius * 0.12));
        shard.lineTo(QPointF(light.radius * 0.16, light.radius * 0.44));
        shard.closeSubpath();
        painter.fillPath(shard, withAlphaF(light.color, light.opacity * 0.48));
        painter.restore();
    }
    painter.end();

    FastGaussianBlur gaussianBlur(3);
    QImage blurred = gaussianBlur.blur(source, blurRadius * layerDevicePixelRatio);
    blurred.setDevicePixelRatio(layerDevicePixelRatio);
    return blurred;
}

class AvatarView final : public QWidget
{
public:
    explicit AvatarView(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_avatar(QStringLiteral(":/resources/avatar/0.jpg"))
    {
        setFixedSize(92, 92);
        setAttribute(Qt::WA_StyledBackground, false);
    }

    void setAvatarPath(const QString& avatarPath)
    {
        QPixmap nextAvatar(avatarPath);
        if (nextAvatar.isNull()) {
            return;
        }
        m_avatar = nextAvatar;
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);

        const QRectF avatarRect = QRectF(rect()).adjusted(1.0, 1.0, -1.0, -1.0);
        QPainterPath path;
        path.addEllipse(avatarRect);
        painter.setClipPath(path);

        if (!m_avatar.isNull()) {
            const QPixmap scaled = m_avatar.scaled(size(),
                                                   Qt::KeepAspectRatioByExpanding,
                                                   Qt::SmoothTransformation);
            const QPoint topLeft((width() - scaled.width()) / 2,
                                 (height() - scaled.height()) / 2);
            painter.drawPixmap(topLeft, scaled);
        } else {
            painter.fillPath(path, ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));
        }

        painter.setClipping(false);
        painter.setPen(QPen(loginAvatarStrokeColor(), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawEllipse(avatarRect);
    }

private:
    QPixmap m_avatar;
};

class AccountHistoryPopup final : public QWidget
{
public:
    explicit AccountHistoryPopup(QWidget* parent = nullptr)
        : QWidget(parent, Qt::Popup | Qt::FramelessWindowHint)
    {
        setAttribute(Qt::WA_TranslucentBackground);
        setMouseTracking(true);
        setFocusPolicy(Qt::StrongFocus);
        setCursor(Qt::PointingHandCursor);

        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            update();
        });
    }

    void setAccounts(const QVector<LoginAccount>& accounts)
    {
        m_accounts = accounts;
        if (m_hoveredRow >= m_accounts.size()) {
            m_hoveredRow = -1;
        }
        setFixedHeight(m_accounts.size() * kAccountPopupRowHeight);
        update();
    }

    void setAccountSelectedCallback(std::function<void(const LoginAccount&)> callback)
    {
        m_accountSelectedCallback = std::move(callback);
    }

    void setAccountDeletedCallback(std::function<void(const QString&)> callback)
    {
        m_accountDeletedCallback = std::move(callback);
    }

protected:
    void mouseMoveEvent(QMouseEvent* event) override
    {
        const int nextHoveredRow = rowAt(event->pos());
        if (m_hoveredRow != nextHoveredRow) {
            m_hoveredRow = nextHoveredRow;
            update();
        }

        QWidget::mouseMoveEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hoveredRow = -1;
        update();
        QWidget::leaveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        const int row = rowAt(event->pos());
        if (event->button() == Qt::LeftButton && row >= 0 && row < m_accounts.size()) {
            if (deleteButtonRect(row).contains(event->pos())) {
                if (m_accountDeletedCallback) {
                    m_accountDeletedCallback(m_accounts.at(row).accountId);
                }
                event->accept();
                return;
            }

            if (m_accountSelectedCallback) {
                m_accountSelectedCallback(m_accounts.at(row));
            }
            close();
            event->accept();
            return;
        }

        QWidget::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        const QRectF backgroundRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        QPainterPath backgroundPath;
        backgroundPath.addRoundedRect(backgroundRect, kAccountPopupRadius, kAccountPopupRadius);
        painter.fillPath(backgroundPath, ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));

        painter.save();
        painter.setClipPath(backgroundPath);
        for (int i = 0; i < m_accounts.size(); ++i) {
            paintAccountRow(&painter, i);
        }
        painter.restore();

        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1.0));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(backgroundPath);
    }

private:
    int rowAt(const QPoint& pos) const
    {
        const int y = pos.y();
        if (y < 0) {
            return -1;
        }
        const int row = y / kAccountPopupRowHeight;
        return row >= 0 && row < m_accounts.size() ? row : -1;
    }

    QRect rowRect(int row) const
    {
        return QRect(0,
                     row * kAccountPopupRowHeight,
                     width(),
                     kAccountPopupRowHeight);
    }

    QRect deleteButtonRect(int row) const
    {
        const QRect rowBounds = rowRect(row);
        return QRect(width() - kAccountPopupDeleteSize - 10,
                     rowBounds.top() + (rowBounds.height() - kAccountPopupDeleteSize) / 2,
                     kAccountPopupDeleteSize,
                     kAccountPopupDeleteSize);
    }

    void paintAccountRow(QPainter* painter, int row) const
    {
        const LoginAccount& account = m_accounts.at(row);
        const QRect rowBounds = rowRect(row);
        const bool hovered = row == m_hoveredRow;

        if (hovered) {
            painter->setPen(Qt::NoPen);
            QColor hoverColor = ThemeManager::instance().color(ThemeColor::ListHover);
            hoverColor = ThemeManager::instance().isDark() ? hoverColor.lighter(132) : hoverColor.darker(112);
            painter->fillRect(rowBounds, hoverColor);
        }

        const QRect avatarRect(15,
                               rowBounds.top() + (rowBounds.height() - kAccountPopupAvatarSize) / 2,
                               kAccountPopupAvatarSize,
                               kAccountPopupAvatarSize);
        QPainterPath avatarPath;
        avatarPath.addEllipse(QRectF(avatarRect));
        painter->save();
        painter->setClipPath(avatarPath);
        const QPixmap avatar(account.avatarPath);
        if (!avatar.isNull()) {
            const QPixmap scaled = avatar.scaled(avatarRect.size(),
                                                 Qt::KeepAspectRatioByExpanding,
                                                 Qt::SmoothTransformation);
            painter->drawPixmap(avatarRect, scaled);
        } else {
            painter->fillPath(avatarPath, ThemeManager::instance().color(ThemeColor::InputBackground));
        }
        painter->restore();

        painter->setPen(QPen(loginAvatarStrokeColor(), 1.0));
        painter->setBrush(Qt::NoBrush);
        painter->drawEllipse(QRectF(avatarRect).adjusted(0.5, 0.5, -0.5, -0.5));

        const int deleteAreaWidth = hovered ? kAccountPopupDeleteSize + 14 : 0;
        const QRect textRect(avatarRect.right() + 10,
                             rowBounds.top(),
                             width() - avatarRect.right() - 20 - deleteAreaWidth,
                             rowBounds.height());
        painter->setFont(AppFonts::applicationPixelWeightedFont(15, QFont::DemiBold));
        painter->setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        painter->drawText(textRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          loginAccountDisplayText(account));

        if (hovered) {
            const QRect closeRect = deleteButtonRect(row);
            QPen closePen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                          1.6,
                          Qt::SolidLine,
                          Qt::RoundCap,
                          Qt::RoundJoin);
            painter->setPen(closePen);
            const QPointF center(closeRect.left() + closeRect.width() / 2.0,
                                 closeRect.top() + closeRect.height() / 2.0);
            painter->drawLine(QPointF(center.x() - 4.0, center.y() - 4.0),
                              QPointF(center.x() + 4.0, center.y() + 4.0));
            painter->drawLine(QPointF(center.x() + 4.0, center.y() - 4.0),
                              QPointF(center.x() - 4.0, center.y() + 4.0));
        }
    }

    QVector<LoginAccount> m_accounts;
    int m_hoveredRow = -1;
    std::function<void(const LoginAccount&)> m_accountSelectedCallback;
    std::function<void(const QString&)> m_accountDeletedCallback;
};

class LoginCheckButton final : public QAbstractButton
{
public:
    explicit LoginCheckButton(const QString& text, QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setText(text);
        setCheckable(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(24);
        setAttribute(Qt::WA_StyledBackground, false);
        setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
        updateWidth();
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        AppFonts::configurePainterForText(painter);

        const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
        const QRect boxRect(0, (height() - 16) / 2, 16, 16);
        painter.setPen(QPen(isChecked() ? accent : ThemeManager::instance().color(ThemeColor::Divider), 1.2));
        painter.setBrush(isChecked()
                         ? accent
                         : (m_hovered ? ThemeManager::instance().color(ThemeColor::ListHover)
                                      : ThemeManager::instance().color(ThemeColor::InputBackground)));
        painter.drawRoundedRect(boxRect.adjusted(0, 0, -1, -1), 4, 4);

        if (isChecked()) {
            QPen checkPen(ThemeManager::instance().color(ThemeColor::TextOnAccent),
                          1.8,
                          Qt::SolidLine,
                          Qt::RoundCap,
                          Qt::RoundJoin);
            painter.setPen(checkPen);
            painter.drawLine(QPointF(boxRect.left() + 4.0, boxRect.center().y()),
                             QPointF(boxRect.left() + 7.0, boxRect.bottom() - 4.0));
            painter.drawLine(QPointF(boxRect.left() + 7.0, boxRect.bottom() - 4.0),
                             QPointF(boxRect.right() - 3.0, boxRect.top() + 4.5));
        }

        painter.setFont(AppFonts::applicationPixelSizedFont(13));
        painter.setPen(m_hovered
                       ? ThemeManager::instance().color(ThemeColor::PrimaryText)
                       : ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter.drawText(rect().adjusted(24, 0, 0, 0), Qt::AlignLeft | Qt::AlignVCenter, text());
    }

private:
    void updateWidth()
    {
        setFixedWidth(24 + QFontMetrics(AppFonts::applicationPixelSizedFont(13)).horizontalAdvance(text()));
    }

    bool m_hovered = false;
};

class SocialLoginButton final : public QAbstractButton
{
public:
    explicit SocialLoginButton(const QString& iconPath, bool tintWithTheme, QWidget* parent = nullptr)
        : QAbstractButton(parent)
        , m_iconPath(iconPath)
        , m_tintWithTheme(tintWithTheme)
        , m_iconPixmap(iconPath)
    {
        setFixedSize(kSocialButtonGeometrySize, kSocialButtonGeometrySize);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_StyledBackground, false);
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

    void mousePressEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton) {
            m_pressed = true;
            update();
        }
        QAbstractButton::mousePressEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        m_pressed = false;
        update();
        QAbstractButton::mouseReleaseEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setOpacity(m_pressed ? 0.62 : (m_hovered ? 0.82 : 1.0));

        const QRectF iconBox((width() - kSocialIconSize) / 2.0,
                             (height() - kSocialIconSize) / 2.0,
                             kSocialIconSize,
                             kSocialIconSize);
        if (!m_iconPixmap.isNull()) {
            QPixmap iconPixmap((QSizeF(size()) * devicePixelRatioF()).toSize());
            iconPixmap.setDevicePixelRatio(devicePixelRatioF());
            iconPixmap.fill(Qt::transparent);

            QPainter iconPainter(&iconPixmap);
            iconPainter.setRenderHint(QPainter::Antialiasing, true);
            iconPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
            iconPainter.drawPixmap(iconBox, m_iconPixmap, QRectF(m_iconPixmap.rect()));
            if (m_tintWithTheme) {
                iconPainter.setCompositionMode(QPainter::CompositionMode_SourceIn);
                iconPainter.fillRect(QRectF(QPointF(0, 0), size()),
                                     ThemeManager::instance().color(ThemeColor::PrimaryText));
            }
            iconPainter.end();

            painter.drawPixmap(QPointF(0.0, 0.0), iconPixmap);
        } else {
            painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
            painter.drawText(rect(), Qt::AlignCenter, m_iconPath.left(1).toUpper());
        }
    }

private:
    QString m_iconPath;
    bool m_tintWithTheme = false;
    QPixmap m_iconPixmap;
    bool m_hovered = false;
    bool m_pressed = false;
};

class LinkButton final : public QAbstractButton
{
public:
    explicit LinkButton(const QString& text, QWidget* parent = nullptr)
        : QAbstractButton(parent)
    {
        setText(text);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFixedHeight(26);
        setAttribute(Qt::WA_StyledBackground, false);
        const int width = QFontMetrics(AppFonts::applicationPixelSizedFont(14))
                                  .horizontalAdvance(text) + 8;
        setFixedWidth(width);
    }

protected:
    void enterEvent(QEnterEvent* event) override
    {
        m_hovered = true;
        update();
        QAbstractButton::enterEvent(event);
    }

    void leaveEvent(QEvent* event) override
    {
        m_hovered = false;
        update();
        QAbstractButton::leaveEvent(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        AppFonts::configurePainterForText(painter);
        painter.setFont(AppFonts::applicationPixelSizedFont(14));
        QColor textColor = ThemeManager::instance().color(m_hovered
                                                          ? ThemeColor::AccentHover
                                                          : ThemeColor::Accent);
        painter.setPen(textColor);
        painter.drawText(rect(), Qt::AlignCenter, text());
    }

private:
    bool m_hovered = false;
};

QLabel* makeTextLabel(const QString& text, int pixelSize, ThemeColor color, QWidget* parent, int weight = QFont::Normal)
{
    auto* label = new QLabel(text, parent);
    label->setAlignment(Qt::AlignCenter);
    label->setFont(AppFonts::applicationPixelWeightedFont(pixelSize, weight));
    label->setAutoFillBackground(false);
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, ThemeManager::instance().color(color));
    label->setPalette(palette);
    return label;
}

void updateLabelColor(QLabel* label, ThemeColor color)
{
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText, ThemeManager::instance().color(color));
    label->setPalette(palette);
}

} // namespace

LoginWindow::LoginWindow(QWidget* parent)
    : SystemWindow(parent)
{
    setFixedSize(kWindowWidth, kWindowHeight);
    setWindowTitle(QStringLiteral("登录"));
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_TranslucentBackground);
    setCompactTrafficLightsEnabled(true);
    updateBackdropTheme();
    setupBackgroundLights();
    setupUi();
    qApp->installEventFilter(this);

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateBackdropTheme();
        updateBackgroundLightColors();
        if (m_loginButton) {
            m_loginButton->setPrimaryStyle();
        }
        requestBackgroundLayerUpdate(true);
        update();
    });
}

LoginWindow::~LoginWindow()
{
    closeRegisterWindow();
    qApp->removeEventFilter(this);
}

void LoginWindow::setupUi()
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(1, 1, 1, 1);
    rootLayout->setSpacing(0);

    m_titleBar = new QWidget(this);
    m_titleBar->setFixedHeight(kTitleBarHeight);
    m_titleBar->setAttribute(Qt::WA_StyledBackground, false);
    setDragTitleBar(m_titleBar);

#ifndef Q_OS_MACOS
    auto* minimizeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Minimize, m_titleBar);
    auto* closeButton = new WindowsWindowControlButton(WindowsWindowControlButton::Kind::Close, m_titleBar);
    closeButton->setGeometry(kWindowWidth - 2 - closeButton->width(),
                             0,
                             closeButton->width(),
                             closeButton->height());
    minimizeButton->setGeometry(closeButton->x() - minimizeButton->width(),
                                0,
                                minimizeButton->width(),
                                minimizeButton->height());
    minimizeButton->raise();
    closeButton->raise();
    connect(minimizeButton, &QAbstractButton::clicked, this, &QWidget::showMinimized);
    connect(closeButton, &QAbstractButton::clicked, this, &QWidget::close);
#endif

    rootLayout->addWidget(m_titleBar);

    auto* content = new QWidget(this);
    content->setAttribute(Qt::WA_StyledBackground, false);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 24, 0, 22);
    contentLayout->setSpacing(0);
    rootLayout->addWidget(content, 1);

    m_avatarView = new AvatarView(content);
    contentLayout->addWidget(m_avatarView, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(34);

    const QVector<LoginAccount> initialAccounts = LoginAccountRepository::instance().requestLoginAccounts({1});
    const LoginAccount initialAccount = initialAccounts.isEmpty() ? LoginAccount{} : initialAccounts.first();

    m_accountField = new LoginInputField(content);
    m_accountField->setFixedWidth(kFormWidth);
    m_accountField->setDropdownEnabled(true);
    m_accountField->setPlaceholderText(QStringLiteral("用户 ID"));
    m_accountField->setText(initialAccount.accountId);
    if (!initialAccount.avatarPath.isEmpty()) {
        static_cast<AvatarView*>(m_avatarView)->setAvatarPath(initialAccount.avatarPath);
    }
    contentLayout->addWidget(m_accountField, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(14);

    m_passwordField = new LoginInputField(content);
    m_passwordField->setFixedWidth(kFormWidth);
    m_passwordField->setPasswordMode(true);
    m_passwordField->setPlaceholderText(QStringLiteral("密码"));
    m_passwordField->setText(initialAccount.password);
    contentLayout->addWidget(m_passwordField, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(6);

    auto* optionsHost = new QWidget(content);
    optionsHost->setFixedWidth(kFormWidth);
    optionsHost->setAttribute(Qt::WA_StyledBackground, false);
    auto* optionsLayout = new QHBoxLayout(optionsHost);
    optionsLayout->setContentsMargins(0, 0, 0, 0);
    optionsLayout->setSpacing(0);
    m_rememberButton = new LoginCheckButton(QStringLiteral("记住密码"), optionsHost);
    m_autoLoginButton = new LoginCheckButton(QStringLiteral("自动登录"), optionsHost);
    m_rememberButton->setChecked(true);
    optionsLayout->addWidget(m_rememberButton);
    optionsLayout->addStretch();
    optionsLayout->addWidget(m_autoLoginButton);
    contentLayout->addWidget(optionsHost, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(8);

    m_errorLabel = makeTextLabel(QString(), 12, ThemeColor::DangerText, content);
    m_errorLabel->setFixedHeight(18);
    contentLayout->addWidget(m_errorLabel);
    contentLayout->addSpacing(8);

    m_loginButton = new StatefulPushButton(QStringLiteral("登录"), content);
    m_loginButton->setFixedSize(kFormWidth, 44);
    m_loginButton->setRadius(10);
    m_loginButton->setPrimaryStyle();
    m_loginButton->setFont(AppFonts::applicationPixelWeightedFont(15, QFont::DemiBold));
    contentLayout->addWidget(m_loginButton, 0, Qt::AlignHCenter);

    contentLayout->addStretch(1);

    auto* socialHost = new QWidget(content);
    socialHost->setFixedSize(kSocialButtonAreaWidth,
                             kSocialButtonGeometrySize);
    socialHost->setAttribute(Qt::WA_StyledBackground, false);
    auto* googleButton = new SocialLoginButton(QStringLiteral(":/resources/icon/google_original.png"), false, socialHost);
    auto* githubButton = new SocialLoginButton(QStringLiteral(":/resources/icon/github_original.png"), true, socialHost);
    auto* appleButton = new SocialLoginButton(QStringLiteral(":/resources/icon/apple_original.png"), true, socialHost);
    googleButton->setGeometry(-kSocialButtonHitInset, 0, kSocialButtonGeometrySize, kSocialButtonGeometrySize);
    githubButton->setGeometry(kSocialButtonSize + kSocialButtonGap - kSocialButtonHitInset,
                              0,
                              kSocialButtonGeometrySize,
                              kSocialButtonGeometrySize);
    appleButton->setGeometry((kSocialButtonSize + kSocialButtonGap) * 2 - kSocialButtonHitInset,
                             0,
                             kSocialButtonGeometrySize,
                             kSocialButtonGeometrySize);
    contentLayout->addWidget(socialHost, 0, Qt::AlignHCenter);
    contentLayout->addSpacing(kSocialBottomSpacing);

    auto* registerButton = new LinkButton(QStringLiteral("注册账号"), content);
    contentLayout->addWidget(registerButton, 0, Qt::AlignHCenter);

    connect(m_accountField, &LoginInputField::dropdownRequested, this, &LoginWindow::showAccountPopup);
    connect(m_rememberButton, &QAbstractButton::toggled, this, [this](bool checked) {
        if (!checked && m_autoLoginButton->isChecked()) {
            m_autoLoginButton->setChecked(false);
        }
    });
    connect(m_autoLoginButton, &QAbstractButton::toggled, this, [this](bool checked) {
        if (checked && !m_rememberButton->isChecked()) {
            m_rememberButton->setChecked(true);
        }
    });
    setTabOrder(m_accountField->lineEdit(), m_passwordField->lineEdit());
    connect(m_passwordField->lineEdit(), &QLineEdit::returnPressed, this, &LoginWindow::attemptLogin);
    connect(m_loginButton, &QPushButton::clicked, this, &LoginWindow::attemptLogin);
    connect(registerButton, &QAbstractButton::clicked, this, &LoginWindow::showRegisterWindow);

    auto updateThemeLabels = [this]() {
        updateLabelColor(m_errorLabel, ThemeColor::DangerText);
    };
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, updateThemeLabels);
}

bool LoginWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress) {
        auto* targetWidget = qobject_cast<QWidget*>(watched);
        if (targetWidget && targetWidget->window() == this) {
            auto* mouseEvent = static_cast<QMouseEvent*>(event);
            const QPoint globalPos = mouseEvent->globalPosition().toPoint();
            const bool insideAccountField = m_accountField
                    && m_accountField->rect().contains(m_accountField->mapFromGlobal(globalPos));
            const bool insidePasswordField = m_passwordField
                    && m_passwordField->rect().contains(m_passwordField->mapFromGlobal(globalPos));
            if (!insideAccountField && !insidePasswordField) {
                if (QWidget* focused = focusWidget()) {
                    focused->clearFocus();
                }
            }
        }
    }

    return SystemWindow::eventFilter(watched, event);
}

void LoginWindow::closeEvent(QCloseEvent* event)
{
    closeRegisterWindow();
    SystemWindow::closeEvent(event);
}

void LoginWindow::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF bounds(rect());
    painter.fillRect(bounds, loginBackgroundWash(bounds, ThemeManager::instance().isDark()));

    if (!m_blurredBackgroundLayer.isNull()) {
        painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        painter.drawImage(rect(), m_blurredBackgroundLayer);
    } else {
    for (const BackgroundLight& light : m_backgroundLights) {
        QColor centerColor = light.color;
        centerColor.setAlphaF(light.currentOpacity * 0.72);
        QColor edgeColor = light.color;
        edgeColor.setAlpha(0);

        QRadialGradient glow(light.currentPosition, light.currentRadius * 1.25);
        glow.setColorAt(0.0, centerColor);
        glow.setColorAt(1.0, edgeColor);
        painter.setBrush(glow);
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(light.currentPosition, light.currentRadius * 1.25, light.currentRadius * 0.92);
    }
}
}

void LoginWindow::showEvent(QShowEvent* event)
{
    SystemWindow::showEvent(event);
    centerOnPrimaryScreen();
    setFocus(Qt::OtherFocusReason);
}

void LoginWindow::updateBackdropTheme()
{
    setBackdropColor(ThemeManager::instance().color(ThemeColor::WindowBackdropTint));
}

void LoginWindow::updateAutoLoginRules()
{
    if (m_autoLoginButton->isChecked() && !m_rememberButton->isChecked()) {
        m_rememberButton->setChecked(true);
    }
}

void LoginWindow::updateAvatarForAccount(const QString& accountId)
{
    if (!m_avatarView) {
        return;
    }

    const LoginAccount account = LoginAccountRepository::instance().requestLoginAccount({accountId.trimmed()});
    if (!account.avatarPath.isEmpty()) {
        static_cast<AvatarView*>(m_avatarView)->setAvatarPath(account.avatarPath);
    }
}

void LoginWindow::attemptLogin()
{
    if (m_loginPending) {
        return;
    }

    updateAutoLoginRules();
    const QString accountId = m_accountField->text().trimmed();
    const QString password = m_passwordField->text();
    const bool credentialsMatch = LoginAccountRepository::instance().validateCredentials(accountId, password);
    if (!credentialsMatch) {
        m_errorLabel->setText(QStringLiteral("账号或密码不正确"));
        GlobalNotification::showFailure(this, QStringLiteral("登录失败"));
        return;
    }

    m_errorLabel->clear();
    m_loginPending = true;
    m_loginButton->setEnabled(false);
    m_loginButton->setText(QStringLiteral("等待主窗口出现"));
    LoginAccountRepository::instance().recordSuccessfulLogin(accountId, password);
    const LoginAccount account = LoginAccountRepository::instance().requestLoginAccount({accountId});
    CurrentUserProfile profile;
    profile.userId = account.accountId;
    profile.nickName = account.displayName.isEmpty() ? account.accountId : account.displayName;
    profile.avatarPath = account.avatarPath;
    profile.status = account.status;
    profile.signature = account.signature;
    profile.region = account.region;
    CurrentUserProfileRepository::instance().saveCurrentUserProfile(profile);
    CurrentUser::instance().setUserInfo(profile.userId);
    GlobalNotification::showSuccess(this, QStringLiteral("登录成功"));

    QTimer::singleShot(kShowMainWindowDelayMs, this, [this]() {
        emit loginAccepted();
    });
}

void LoginWindow::showRegisterWindow()
{
    if (m_registerWindow) {
        m_registerWindow->raise();
        m_registerWindow->activateWindow();
        return;
    }

    auto* window = new RegisterWindow(this);
    m_registerWindow = window;
    connect(window, &RegisterWindow::accountRegistered, this, [this](const QString& accountId,
                                                                     const QString& password) {
        if (m_accountField) {
            m_accountField->setText(accountId);
        }
        if (m_passwordField) {
            m_passwordField->setText(password);
            m_passwordField->lineEdit()->setFocus(Qt::OtherFocusReason);
        }
        updateAvatarForAccount(accountId);
        m_errorLabel->clear();
    });
    connect(window, &QObject::destroyed, this, [this]() {
        m_registerWindow = nullptr;
    });
    window->show();
    window->raise();
    window->activateWindow();
}

void LoginWindow::closeRegisterWindow()
{
    if (!m_registerWindow) {
        return;
    }

    RegisterWindow* window = m_registerWindow;
    m_registerWindow = nullptr;
    window->close();
}

void LoginWindow::showAccountPopup()
{
    if (!m_accountField) {
        return;
    }

    auto* popup = static_cast<AccountHistoryPopup*>(m_accountPopup);
    if (!popup) {
        popup = new AccountHistoryPopup(this);
        m_accountPopup = popup;
        popup->setAccountSelectedCallback([this](const LoginAccount& account) {
            if (m_accountField) {
                m_accountField->setText(account.accountId);
            }
            updateAvatarForAccount(account.accountId);
            if (m_passwordField) {
                m_passwordField->setText(account.password);
                m_passwordField->lineEdit()->setFocus(Qt::OtherFocusReason);
            }
        });
        popup->setAccountDeletedCallback([this](const QString& accountId) {
            LoginAccountRepository::instance().removeLoginAccount(accountId);
            showAccountPopup();
        });
        connect(popup, &QObject::destroyed, this, [this]() {
            m_accountPopup = nullptr;
        });
    }

    popup->setAccounts(LoginAccountRepository::instance().requestLoginAccounts({kAccountPopupMaxRows}));
    const int popupWidth = m_accountField->width();
    popup->setFixedWidth(popupWidth);
    auto positionPopup = [this, popup]() {
        if (!m_accountField || !popup) {
            return;
        }
        popup->move(m_accountField->mapToGlobal(QPoint(0, m_accountField->height() + kAccountPopupGap)));
    };

    positionPopup();
    popup->show();
    positionPopup();
    QTimer::singleShot(0, popup, positionPopup);
    popup->raise();
    popup->setFocus(Qt::PopupFocusReason);
}

void LoginWindow::centerOnPrimaryScreen()
{
    QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    move(available.center() - rect().center());
}

void LoginWindow::setupBackgroundLights()
{
    m_backgroundLights.clear();
    const QVector<QColor> colors = loginBackgroundColors();

    for (const QColor& color : colors) {
        const qreal radius = randomRange(235.0, 385.0);
        const qreal targetRadius = randomRange(235.0, 385.0);
        const QPointF position = randomBackgroundPoint(radius);
        BackgroundLight light;
        light.color = color;
        light.startPosition = position;
        light.currentPosition = position;
        light.targetPosition = randomBackgroundPoint(targetRadius);
        light.startRadius = radius;
        light.currentRadius = radius;
        light.targetRadius = targetRadius;
        light.startOpacity = randomRange(0.36, 0.50);
        light.currentOpacity = light.startOpacity;
        light.targetOpacity = randomRange(0.36, 0.50);
        light.progress = randomRange(0.0, 0.85);
        const qreal eased = easeInOut(light.progress);
        light.currentPosition = light.startPosition
                + (light.targetPosition - light.startPosition) * eased;
        light.currentRadius = light.startRadius
                + (light.targetRadius - light.startRadius) * eased;
        light.currentOpacity = light.startOpacity
                + (light.targetOpacity - light.startOpacity) * eased;
        light.durationMs = randomRange(7200.0, 12800.0);
        light.rotation = randomRange(0.0, 360.0);
        light.rotationSpeed = randomRange(-7.5, 7.5);
        m_backgroundLights.append(light);
    }

    m_backgroundClock.start();
    m_lastBackgroundTick = m_backgroundClock.elapsed();
    m_backgroundTimer = new QTimer(this);
    m_backgroundTimer->setTimerType(Qt::PreciseTimer);
    m_backgroundTimer->setInterval(kBackgroundFrameMs);
    connect(m_backgroundTimer, &QTimer::timeout, this, &LoginWindow::advanceBackgroundLights);
    m_backgroundTimer->start();
    requestBackgroundLayerUpdate(true);
}

void LoginWindow::updateBackgroundLightColors()
{
    const QVector<QColor> colors = loginBackgroundColors();
    for (int i = 0; i < m_backgroundLights.size(); ++i) {
        m_backgroundLights[i].color = colors.at(i % colors.size());
    }
}

void LoginWindow::advanceBackgroundLights()
{
    if (m_backgroundLights.isEmpty()) {
        return;
    }

    const qint64 now = m_backgroundClock.elapsed();
    const qreal deltaMs = qMax<qint64>(0, now - m_lastBackgroundTick);
    m_lastBackgroundTick = now;

    for (BackgroundLight& light : m_backgroundLights) {
        light.rotation += light.rotationSpeed * deltaMs / 1000.0;
        light.progress += deltaMs / light.durationMs;
        if (light.progress >= 1.0) {
            light.startPosition = light.targetPosition;
            light.startRadius = light.targetRadius;
            light.startOpacity = light.targetOpacity;
            light.targetRadius = randomRange(235.0, 385.0);
            light.targetPosition = randomBackgroundPoint(light.targetRadius);
            light.targetOpacity = randomRange(0.36, 0.50);
            light.durationMs = randomRange(7200.0, 12800.0);
            light.rotationSpeed = randomRange(-7.5, 7.5);
            light.progress -= 1.0;
        }

        const qreal eased = easeInOut(light.progress);
        light.currentPosition = light.startPosition
                + (light.targetPosition - light.startPosition) * eased;
        light.currentRadius = light.startRadius
                + (light.targetRadius - light.startRadius) * eased;
        light.currentOpacity = light.startOpacity
                + (light.targetOpacity - light.startOpacity) * eased;
    }

    requestBackgroundLayerUpdate(false);
    update();
}

QPointF LoginWindow::randomBackgroundPoint(qreal radius) const
{
    const qreal horizontalMargin = radius * 0.52;
    const qreal verticalMargin = radius * 0.42;
    return QPointF(randomRange(-horizontalMargin, width() + horizontalMargin),
                   randomRange(kTitleBarHeight - verticalMargin, height() + verticalMargin));
}

void LoginWindow::requestBackgroundLayerUpdate(bool force)
{
    if (!size().isValid() || m_backgroundLights.isEmpty()) {
        return;
    }

    const qint64 now = m_backgroundClock.isValid() ? m_backgroundClock.elapsed() : 0;
    if (!force && now - m_lastBackgroundLayerRequest < kBackgroundLayerRefreshMs) {
        return;
    }

    if (m_backgroundLayerInFlight) {
        m_backgroundLayerUpdatePending = true;
        return;
    }

    m_lastBackgroundLayerRequest = now;
    m_backgroundLayerInFlight = true;
    m_backgroundLayerUpdatePending = false;
    const int generation = ++m_backgroundLayerGeneration;

    QVector<BackgroundLightFrame> frames;
    frames.reserve(m_backgroundLights.size());
    for (const BackgroundLight& light : m_backgroundLights) {
        BackgroundLightFrame frame;
        frame.color = light.color;
        frame.position = light.currentPosition;
        frame.radius = light.currentRadius;
        frame.opacity = light.currentOpacity;
        frame.rotation = light.rotation;
        frames.append(frame);
    }

    auto* watcher = new QFutureWatcher<QImage>(this);
    m_backgroundLayerWatcher = watcher;
    connect(watcher, &QFutureWatcher<QImage>::finished, this, [this, watcher, generation]() {
        const QImage layer = watcher->result();
        if (m_backgroundLayerWatcher == watcher) {
            m_backgroundLayerWatcher = nullptr;
        }
        watcher->deleteLater();

        m_backgroundLayerInFlight = false;
        if (generation == m_backgroundLayerGeneration && !layer.isNull()) {
            m_blurredBackgroundLayer = layer;
            update();
        }

        if (m_backgroundLayerUpdatePending) {
            m_backgroundLayerUpdatePending = false;
            requestBackgroundLayerUpdate(true);
        }
    });

    watcher->setFuture(QtConcurrent::run(renderBlurredLoginBackground,
                                         size(),
                                         devicePixelRatioF(),
                                         kBackgroundBlurRenderScale,
                                         kBackgroundBlurRadius,
                                         frames,
                                         ThemeManager::instance().isDark()));
}
