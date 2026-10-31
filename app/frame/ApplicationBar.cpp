#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "app/state/CurrentUserProfileEditContent.h"
#include "app/state/CurrentUserStatusPopupContent.h"
#include "features/aichat/data/AiChatRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/FriendNotificationRepository.h"
#include "features/friend/data/GroupNotificationRepository.h"
#include "features/friend/ui/FriendProfilePopup.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "shared/theme/ThemeManager.h"
#include "app/state/CurrentUser.h"
#include <QHBoxLayout>
#include <QIcon>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>

namespace {

constexpr int kAvatarStatusCutoutSize = 18;
constexpr int kAvatarStatusIconSize = 13;
constexpr int kAvatarStatusPopupGap = 8;
constexpr int kAvatarStatusCenterOffset = -2;
constexpr int kStatusIconChoiceCount = 4;

} // namespace

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);

#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    setAvatarSource(CurrentUser::instance().getAvatarPath());
    connect(&CurrentUser::instance(), &CurrentUser::identityChanged, this, [this]() {
        setAvatarSource(CurrentUser::instance().getAvatarPath());
        update();
    });

    connect(&ImageService::instance(), &ImageService::previewReady, this, [this]() {
        update(avatarRect());
    });

    messageItem = new ApplicationBarItem(
            ":/resources/icon/unselected_message.png",
            ":/resources/icon/selected_message.png");
    addItem(messageItem);
    friendItem = new ApplicationBarItem(
            ":/resources/icon/friend_unselected.png",
            ":/resources/icon/friend_selected.png");
    friendItem->setPixmapScale(0.62);
    addItem(friendItem);

    auto momentItem = new ApplicationBarItem(
            ":/resources/icon/unselected_blazer.png",
            ":/resources/icon/blazer.png");
    momentItem->setPixmapScale(0.68);
    addItem(momentItem);

    aiChatItem = new ApplicationBarItem(
            ":/resources/icon/unselected_aichat.png",
            ":/resources/icon/aichat.png");
    aiChatItem->setPixmapScale(0.77);
    addItem(aiChatItem);

    auto notItem = new ApplicationBarItem(
            ":/resources/icon/unselected_nether.png",
            ":/resources/icon/nether.png");
    notItem->setPixmapScale(0.72);
    notItem->setDarkModeInversionEnabled(false);
    addItem(notItem);

    moreOptionsItem = new ApplicationBarItem(":/resources/icon/menu.png");
    moreOptionsItem->setPixmapScale(0.57);
    addBottomItem(moreOptionsItem);

    if (!topItems.empty()) {
        selectedItem = topItems[0];
        topItems[0]->setSelected(true);
        highlightPosY = selectedItem->y();
    }
    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(250);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        highlightPosY = value.toInt();
        update();
    });

    connect(&MessageRepository::instance(), &MessageRepository::conversationListChanged,
            this, [this](const QString&) { refreshChatBadge(); });
    connect(&AiChatRepository::instance(), &AiChatRepository::unreadDotStateChanged,
            this, [this]() { refreshAiChatBadge(); });
    connect(&FriendNotificationRepository::instance(), &FriendNotificationRepository::notificationListChanged,
            this, [this]() { refreshFriendBadge(); });
    connect(&GroupNotificationRepository::instance(), &GroupNotificationRepository::notificationListChanged,
            this, [this]() { refreshFriendBadge(); });
    refreshChatBadge();
    refreshAiChatBadge();
    refreshFriendBadge();
}

void ApplicationBar::resizeEvent(QResizeEvent*) {
    layoutItems();
}

void ApplicationBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
#ifdef Q_OS_WIN
    QColor windowColor = ThemeManager::instance().color(ThemeColor::PanelBackground);
    windowColor.setAlpha(128);
    painter.fillRect(rect(), windowColor);
#endif
    int w = width();
    if (selectedItem) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::AppBarItemSelectedBackground));
        int x = (width() - iconSize) / 2;
        QRect r(x, highlightPosY, iconSize, iconSize);
        painter.drawRoundedRect(r, 10, 10);
        painter.restore();
    }

    const QRect avatar = avatarRect();
    if (!avatarSource.isEmpty()) {
        painter.save();
        const qreal dpr = painter.device()->devicePixelRatioF();
        const QPixmap avatarPixmap = ImageService::instance().circularAvatar(avatarSource,
                                                                             avatarSize,
                                                                             dpr);
        QPainterPath avatarPath;
        avatarPath.addEllipse(avatar);
        QPainterPath statusCutoutPath;
        statusCutoutPath.addEllipse(avatarCutoutRect());
        painter.setClipPath(avatarPath.subtracted(statusCutoutPath));
        if (avatarPixmap.isNull()) {
            MediaPlaceholderRenderer::drawAvatar(&painter, avatar);
        } else {
            painter.drawPixmap(avatar, avatarPixmap);
        }
        painter.restore();

        painter.save();
        const QRect status = avatarStatusIconRect();
        QPainterPath statusPath;
        statusPath.addEllipse(status);
        painter.setClipPath(statusPath);
        const QPixmap statusPixmap = ImageService::instance().scaled(avatarStatusIconSource(),
                                                                     status.size(),
                                                                     Qt::KeepAspectRatio,
                                                                     painter.device()->devicePixelRatioF());
        if (!statusPixmap.isNull()) {
            painter.drawPixmap(status, statusPixmap);
        }
        painter.restore();
    }

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        item->paint(painter);
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        item->paint(painter);
    }
}

void ApplicationBar::addItem(ApplicationBarItem* item)
{
    if (!item) {
        return;
    }

    topItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
    layoutItems();
}

void ApplicationBar::setAvatarSource(const QString& source)
{
    avatarSource = source;
    layoutItems();
}

void ApplicationBar::setTopInset(int inset)
{
    const int clampedInset = qMax(0, inset);
    if (topInset == clampedInset) {
        return;
    }

    topInset = clampedInset;
    layoutItems();
    update();
}

void ApplicationBar::setCurrentTopIndex(int index)
{
    if (index < 0 || index >= topItems.size()) {
        return;
    }

    onItemClicked(topItems.at(index));
}


void ApplicationBar::layoutItems() {
    int y = topInset + marginTop + spacing + avatarSize + avatarAndItemDist;
    int w = width();

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, y, iconSize, iconSize));
        y += iconSize + spacing;
    }
    int yb = height() - marginBottom - iconSize;
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, yb, iconSize, iconSize));
        yb -= (iconSize + spacing);
    }

    if (selectedItem && highlightAnim->state() != QAbstractAnimation::Running) {
        highlightPosY = selectedItem->y();
    }
}

void ApplicationBar::refreshChatBadge()
{
    if (!messageItem) {
        return;
    }

    int totalPromptUnreadCount = 0;
    const QVector<ConversationSummary> conversations =
            MessageRepository::instance().requestConversationList();
    for (const ConversationSummary& conversation : conversations) {
        if (conversation.isDoNotDisturb) {
            continue;
        }

        totalPromptUnreadCount += qMax(0, conversation.unreadCount);
    }
    messageItem->setBadgeCount(totalPromptUnreadCount);
}

void ApplicationBar::refreshAiChatBadge()
{
    if (!aiChatItem) {
        return;
    }

    aiChatItem->setBadgeDotVisible(AiChatRepository::instance().unreadDotCount() > 0);
}

void ApplicationBar::refreshFriendBadge()
{
    if (!friendItem) {
        return;
    }

    const int totalUnreadCount =
            qMax(0, FriendNotificationRepository::instance().unreadCount()) +
            qMax(0, GroupNotificationRepository::instance().unreadCount());
    friendItem->setBadgeCount(totalUnreadCount);
}

QRect ApplicationBar::avatarRect() const
{
    const int x = (width() - avatarSize) / 2;
    const int y = topInset + marginTop + spacing;
    return QRect(x, y, avatarSize, avatarSize);
}

QRect ApplicationBar::avatarStatusRect() const
{
    const QRect avatar = avatarRect();
    const QPoint center(avatar.right() + kAvatarStatusCenterOffset,
                        avatar.bottom() + kAvatarStatusCenterOffset);
    return QRect(center.x() - kAvatarStatusCutoutSize / 2,
                 center.y() - kAvatarStatusCutoutSize / 2,
                 kAvatarStatusCutoutSize,
                 kAvatarStatusCutoutSize);
}

QRect ApplicationBar::avatarCutoutRect() const
{
    return avatarStatusRect();
}

QRect ApplicationBar::avatarStatusIconRect() const
{
    const QPoint center = avatarStatusRect().center();
    return QRect(center.x() - kAvatarStatusIconSize / 2,
                 center.y() - kAvatarStatusIconSize / 2,
                 kAvatarStatusIconSize,
                 kAvatarStatusIconSize);
}

void ApplicationBar::onItemClicked(ApplicationBarItem* item)
{
    if (!item || !topItems.contains(item) || selectedItem == item)
        return;

    if (selectedItem) selectedItem->setSelected(false);
    item->setSelected(true);

    int startY = highlightPosY;
    int endY = item->y();
    highlightAnim->stop();
    highlightAnim->setStartValue(startY);
    highlightAnim->setEndValue(endY);
    highlightAnim->start();

    selectedItem = item;
    emit applicationClicked(item);
}

void ApplicationBar::addBottomItem(ApplicationBarItem* item)
{
    if (!item) {
        return;
    }

    bottomItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
    layoutItems();
}

void ApplicationBar::mouseMoveEvent(QMouseEvent* event)
{
    setHoveredItem(itemAtPosition(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void ApplicationBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        if (!avatarSource.isEmpty()) {
            if (avatarStatusRect().contains(event->pos())) {
                showCurrentUserStatusPopup();
                event->accept();
                return;
            }
            if (avatarRect().contains(event->pos())) {
                showCurrentUserProfilePopup();
                event->accept();
                return;
            }
        }

        ApplicationBarItem* item = itemAtPosition(event->pos());
        if (item == moreOptionsItem) {
            showMoreOptionsMenu();
        } else {
            onItemClicked(item);
        }
    }

    QWidget::mousePressEvent(event);
}

void ApplicationBar::leaveEvent(QEvent* event)
{
    setHoveredItem(nullptr);
    QWidget::leaveEvent(event);
}

ApplicationBarItem* ApplicationBar::itemAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    return nullptr;
}

void ApplicationBar::showCurrentUserProfilePopup()
{
    const QString userId = CurrentUser::instance().getUserId();
    if (userId.isEmpty()) {
        return;
    }

    if (!currentUserProfilePopup) {
        currentUserProfilePopup = new FriendProfilePopup(this);
        connect(currentUserProfilePopup,
                &FriendProfilePopup::requestEditProfile,
                this,
                [this]() {
                    QTimer::singleShot(0, this, &ApplicationBar::showCurrentUserEditProfilePopup);
                });
    }

    const QRect avatar = avatarRect();
    const QPoint popupAnchor = mapToGlobal(QPoint(avatar.right()
                                                  + currentUserProfilePopup->width()
                                                  + kAvatarStatusPopupGap,
                                                  avatar.top()));
    currentUserProfilePopup->popupAt(popupAnchor, userId);
}

void ApplicationBar::showCurrentUserEditProfilePopup()
{
    if (currentUserEditProfilePopup) {
        currentUserEditProfilePopup->raise();
        return;
    }

    const CurrentUserProfile profile = CurrentUser::instance().profile();
    if (!profile.isValid()) {
        return;
    }

    auto* content = new CurrentUserProfileEditContent(profile);
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(520, 520);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    currentUserEditProfilePopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!currentUserEditProfilePopup) {
        return;
    }

    content->saveRequested = [this](const CurrentUserProfile& editedProfile) {
        CurrentUser::instance().saveProfile(editedProfile);
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    };
    content->cancelRequested = [this]() {
        if (currentUserEditProfilePopup) {
            currentUserEditProfilePopup->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    };

    connect(currentUserEditProfilePopup,
            &InWindowPopupOverlay::dismissed,
            this,
            [this]() {
                currentUserEditProfilePopup = nullptr;
            });

    content->setFocus(Qt::PopupFocusReason);
}

void ApplicationBar::showCurrentUserStatusPopup()
{
    if (CurrentUser::instance().getUserId().isEmpty()) {
        return;
    }

    if (currentUserStatusPopup) {
        currentUserStatusPopup->raise();
        return;
    }

    auto* content = new CurrentUserStatusPopupContent(avatarStatusChoiceIndex());
    content->selectionChanged = [this](int index) {
        setAvatarStatusChoiceIndex(index);
    };
    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(760, 420);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    currentUserStatusPopup = InWindowPopupOverlay::showPopup(this, content, options);
    if (!currentUserStatusPopup) {
        return;
    }

    connect(currentUserStatusPopup, &InWindowPopupOverlay::dismissed, this, [this]() {
        currentUserStatusPopup = nullptr;
    });
}

QString ApplicationBar::avatarStatusIconSource() const
{
    if (privateInvisibleStatus) {
        return QStringLiteral(":/resources/icon/invisible.png");
    }
    return statusIconPath(CurrentUser::instance().getStatus());
}

int ApplicationBar::avatarStatusChoiceIndex() const
{
    if (privateInvisibleStatus) {
        return 3;
    }

    const UserStatus status = CurrentUser::instance().getStatus();
    if (status == Mining) {
        return 1;
    }
    if (status == Flying) {
        return 2;
    }
    return 0;
}

void ApplicationBar::setAvatarStatusChoiceIndex(int index)
{
    if (index < 0 || index >= kStatusIconChoiceCount) {
        return;
    }

    if (index == 3) {
        privateInvisibleStatus = true;
        update();
        return;
    }

    privateInvisibleStatus = false;
    const UserStatus status = index == 1 ? Mining : (index == 2 ? Flying : Online);
    CurrentUserProfile profile = CurrentUser::instance().profile();
    if (profile.isValid() && profile.status != status) {
        profile.status = status;
        CurrentUser::instance().saveProfile(profile);
    }
    update();
}

void ApplicationBar::setHoveredItem(ApplicationBarItem* item)
{
    if (hoveredItem == item) {
        return;
    }

    if (hoveredItem) {
        hoveredItem->setHovered(false);
    }
    hoveredItem = item;
    if (hoveredItem) {
        hoveredItem->setHovered(true);
    }
}

void ApplicationBar::showMoreOptionsMenu()
{
    if (!moreOptionsItem) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(ThemeManager::instance().color(ThemeColor::ContextMenuHover));
    QAction* settingsAction = menu->addAction(QStringLiteral("设置"));
    QAction* themeColorAction = menu->addAction(QStringLiteral("主题颜色"));
    menu->addAction(QStringLiteral("聊天记录管理"));
    menu->addSeparator();
    QAction* logoutAction = menu->addAction(QStringLiteral("退出登录"));

    connect(settingsAction, &QAction::triggered, this, &ApplicationBar::settingsRequested);
    connect(themeColorAction, &QAction::triggered, this, &ApplicationBar::appearanceSettingsRequested);
    connect(logoutAction, &QAction::triggered, this, &ApplicationBar::logoutRequested);

    connect(menu, &StyledActionMenu::aboutToHide, this, [this, menu]() {
        if (moreOptionsItem) {
            moreOptionsItem->setSelected(false);
        }
        menu->deleteLater();
    });

    moreOptionsItem->setSelected(true);
    const QRect itemRect = moreOptionsItem->rect();
    const int horizontalOffset = menu->isUsingNativeMenu() ? 10 : 6;
    const int verticalOffset = menu->isUsingNativeMenu() ? -22 : -46;
    const QPoint popupPos = mapToGlobal(
            QPoint(itemRect.right() + horizontalOffset,
                   itemRect.y() - itemRect.height() + verticalOffset));
    menu->popupWhenMouseReleased(popupPos);
}
