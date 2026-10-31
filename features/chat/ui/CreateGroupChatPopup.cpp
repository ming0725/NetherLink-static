#include "CreateGroupChatPopup.h"

#include <QDateTime>
#include <QCollator>
#include <QCursor>
#include <QEventLoop>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QScrollBar>
#include <QTimer>
#include <QUuid>
#include <QVariantAnimation>

#include <utility>

#include "app/state/CurrentUser.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/friend/data/UserRepository.h"
#include "shared/services/AppFonts.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"

namespace {

constexpr auto kRecentGroupId = "__recent_chats";
constexpr int kPopupWidth = 660;
constexpr int kPopupHeight = 480;
constexpr int kPadding = 18;
constexpr int kSearchHeight = 30;
constexpr int kFooterHeight = 58;
constexpr int kGroupHeaderHeight = 40;
constexpr int kGroupTitleFontSize = 14;
constexpr int kGroupCountFontSize = 12;
constexpr int kContactItemHeight = 64;
constexpr int kSelectedItemHeight = 56;
constexpr int kAvatarSize = 38;
constexpr int kSelectCircleSize = 17;
constexpr int kLeftListCircleLeft = 14;
constexpr int kGroupArrowSize = 12;
constexpr int kLeftPaneWidth = 304;
constexpr int kPopupRadius = 12;
constexpr int kSearchIconMarginLeft = 5;
constexpr int kSearchClearButtonMarginRight = 5;
constexpr int kSearchClearButtonSize = 20;
constexpr int kGroupCountRightPadding = 18;

void makeListViewTransparent(QListView* view)
{
    if (!view) {
        return;
    }

    view->setAutoFillBackground(false);
    view->setAttribute(Qt::WA_TranslucentBackground, true);
    view->setAttribute(Qt::WA_OpaquePaintEvent, false);

    if (QWidget* listViewport = view->viewport()) {
        listViewport->setAutoFillBackground(false);
        listViewport->setAttribute(Qt::WA_TranslucentBackground, true);
        listViewport->setAttribute(Qt::WA_OpaquePaintEvent, false);
    }

    QPalette listPalette = view->palette();
    listPalette.setColor(QPalette::Base, Qt::transparent);
    listPalette.setColor(QPalette::Window, Qt::transparent);
    view->setPalette(listPalette);
}

QFont textFont(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    return AppFonts::applicationPixelWeightedFont(pixelSize, weight);
}

QFontMetrics textMetrics(int pixelSize, QFont::Weight weight = QFont::Normal)
{
    return AppFonts::applicationPixelWeightedMetrics(pixelSize, weight);
}

bool matchesFriend(const FriendSummary& contact, const QString& keyword)
{
    if (keyword.isEmpty()) {
        return true;
    }

    return contact.displayName.contains(keyword, Qt::CaseInsensitive) ||
           contact.nickName.contains(keyword, Qt::CaseInsensitive) ||
           contact.remark.contains(keyword, Qt::CaseInsensitive) ||
           contact.signature.contains(keyword, Qt::CaseInsensitive) ||
           contact.groupName.contains(keyword, Qt::CaseInsensitive);
}

FriendSummary friendSummaryFromUser(const User& user)
{
    if (user.id.isEmpty()) {
        return {};
    }

    const QString groupId = user.friendGroupId.isEmpty() ? QStringLiteral("default") : user.friendGroupId;
    const QString groupName = user.friendGroupName.isEmpty() ? QStringLiteral("默认分组") : user.friendGroupName;
    return FriendSummary{
            user.id,
            user.remark.isEmpty() ? user.nick : user.remark,
            user.avatarPath,
            user.status,
            user.signature,
            user.isDnd,
            groupId,
            groupName,
            user.nick,
            user.remark
    };
}

class CreateGroupChatSearchInput final : public IconLineEdit
{
public:
    explicit CreateGroupChatSearchInput(QWidget* parent = nullptr)
        : IconLineEdit(parent)
    {
        applyPopupPalette();
        connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
            applyPopupPalette();
            update();
        });
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing, true);
        const QRectF inputRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::CreateGroupPopupBackground));
        painter.setPen(QPen(QWidget::hasFocus()
                            ? ThemeManager::instance().color(ThemeColor::Accent)
                            : ThemeManager::instance().color(ThemeColor::CreateGroupPopupDivider),
                            1));
        painter.drawRoundedRect(inputRect, 8, 8);
        painter.end();

        QLineEdit::paintEvent(event);

        QPainter foregroundPainter(this);
        foregroundPainter.setRenderHint(QPainter::SmoothPixmapTransform, true);
        const QSize iconSize(15, 15);
        const QRect iconRect(kSearchIconMarginLeft,
                             (height() - iconSize.height()) / 2,
                             iconSize.width(),
                             iconSize.height());
        const QString searchIcon = ThemeManager::instance().isDark()
                ? QStringLiteral(":/resources/icon/search_darkmode.png")
                : QStringLiteral(":/resources/icon/search.png");
        foregroundPainter.drawPixmap(iconRect,
                                      ImageService::instance().scaled(searchIcon,
                                                                      iconSize,
                                                                      Qt::IgnoreAspectRatio,
                                                                      foregroundPainter.device()->devicePixelRatioF()));

        if (!text().isEmpty()) {
            const QRect clearButtonRect(width() - kSearchClearButtonMarginRight - kSearchClearButtonSize,
                                        (height() - kSearchClearButtonSize) / 2,
                                        kSearchClearButtonSize,
                                        kSearchClearButtonSize);
            const QSize clearIconSize(12, 12);
            const QRect clearIconRect(clearButtonRect.x() + (clearButtonRect.width() - clearIconSize.width()) / 2,
                                      clearButtonRect.y() + (clearButtonRect.height() - clearIconSize.height()) / 2,
                                      clearIconSize.width(),
                                      clearIconSize.height());
            const QString closeIcon = ThemeManager::instance().isDark()
                    ? QStringLiteral(":/resources/icon/hovered_close.png")
                    : QStringLiteral(":/resources/icon/close.png");
            foregroundPainter.drawPixmap(clearIconRect,
                                          ImageService::instance().scaled(closeIcon,
                                                                          clearIconSize,
                                                                          Qt::IgnoreAspectRatio,
                                                                          foregroundPainter.device()->devicePixelRatioF()));
        }
    }

private:
    void applyPopupPalette()
    {
        QPalette inputPalette = palette();
        inputPalette.setColor(QPalette::Base, Qt::transparent);
        inputPalette.setColor(QPalette::Window, Qt::transparent);
        inputPalette.setColor(QPalette::Text, ThemeManager::instance().color(ThemeColor::CreateGroupPopupPrimaryText));
        inputPalette.setColor(QPalette::PlaceholderText, ThemeManager::instance().color(ThemeColor::CreateGroupPopupTertiaryText));
        inputPalette.setColor(QPalette::Highlight, ThemeManager::instance().color(ThemeColor::Accent));
        inputPalette.setColor(QPalette::HighlightedText, ThemeManager::instance().color(ThemeColor::TextOnAccent));
        setPalette(inputPalette);
    }
};

void drawGroupArrow(QPainter* painter, const QRect& rect, qreal progress)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);
    painter->translate(rect.center());
    painter->rotate(progress * 90.0);
    painter->setPen(QPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupTertiaryText), 1.5,
                         Qt::SolidLine,
                         Qt::RoundCap,
                         Qt::RoundJoin));
    painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
    painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
    painter->restore();
}

GroupRole groupRoleForUser(const Group& group, const QString& userId)
{
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

QString groupMemberName(const Group& group, const User& user)
{
    const QString nickname = group.memberNicknames.value(user.id).trimmed();
    if (!nickname.isEmpty()) {
        return nickname;
    }
    const CurrentUser& currentUser = CurrentUser::instance();
    if (currentUser.isCurrentUserId(user.id)) {
        return currentUser.getUserName();
    }
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    return user.nick.trimmed().isEmpty() ? user.id : user.nick.trimmed();
}

QVector<FriendSummary> groupMembersAsContacts(const Group& group)
{
    const CurrentUser& currentUser = CurrentUser::instance();
    QStringList userIds;
    userIds.reserve(group.membersID.size());
    for (const QString& userId : group.membersID) {
        if (!userId.isEmpty() && !currentUser.isCurrentUserId(userId)) {
            userIds.push_back(userId);
        }
    }

    QHash<QString, User> usersById;
    for (const User& user : UserRepository::instance().requestUserDetails(userIds)) {
        usersById.insert(user.id, user);
    }

    QVector<User> users;
    users.reserve(group.membersID.size());
    for (const QString& userId : group.membersID) {
        User user;
        if (currentUser.isCurrentUserId(userId)) {
            const CurrentUserProfile profile = currentUser.identity();
            user.id = profile.userId;
            user.nick = profile.nickName;
            user.avatarPath = profile.avatarPath;
            user.status = profile.status;
        } else {
            user = usersById.value(userId);
        }
        if (!user.id.isEmpty()) {
            users.push_back(user);
        }
    }

    QCollator collator(QLocale::Chinese);
    collator.setNumericMode(true);
    std::sort(users.begin(), users.end(), [&group, &collator](const User& lhs, const User& rhs) {
        const GroupRole lhsRole = groupRoleForUser(group, lhs.id);
        const GroupRole rhsRole = groupRoleForUser(group, rhs.id);
        const int lhsRank = lhsRole == GroupRole::Owner ? 0 : (lhsRole == GroupRole::Admin ? 1 : 2);
        const int rhsRank = rhsRole == GroupRole::Owner ? 0 : (rhsRole == GroupRole::Admin ? 1 : 2);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        const int nameOrder = collator.compare(groupMemberName(group, lhs), groupMemberName(group, rhs));
        return nameOrder == 0 ? lhs.id < rhs.id : nameOrder < 0;
    });

    QVector<FriendSummary> contacts;
    contacts.reserve(users.size());
    for (const User& user : users) {
        FriendSummary contact = friendSummaryFromUser(user);
        contact.displayName = groupMemberName(group, user);
        contacts.push_back(contact);
    }
    return contacts;
}

QSet<QString> removableDisabledUserIds(const Group& group)
{
    QSet<QString> disabled;
    const QString currentUserId = CurrentUser::instance().getUserId();
    const GroupRole currentRole = groupRoleForUser(group, currentUserId);
    for (const QString& userId : group.membersID) {
        const GroupRole targetRole = groupRoleForUser(group, userId);
        const bool removable = !userId.isEmpty() &&
                               userId != currentUserId &&
                               ((currentRole == GroupRole::Owner && targetRole != GroupRole::Owner) ||
                                (currentRole == GroupRole::Admin && targetRole == GroupRole::Member));
        if (!removable) {
            disabled.insert(userId);
        }
    }
    return disabled;
}

void drawSelectionCircle(QPainter* painter, const QRect& rect, bool selected, bool disabled = false)
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
    QColor border = selected ? accent : ThemeManager::instance().color(ThemeColor::CreateGroupPopupTertiaryText);
    if (disabled) {
        border = ThemeManager::instance().color(ThemeColor::CreateGroupPopupDivider);
    }
    painter->setPen(QPen(border, 1.6));
    painter->setBrush(Qt::NoBrush);
    painter->drawEllipse(QRectF(rect).adjusted(1.0, 1.0, -1.0, -1.0));
    if (selected && !disabled) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawEllipse(QRectF(rect).adjusted(4.5, 4.5, -4.5, -4.5));
    }
    painter->restore();
}

void drawAvatar(QPainter* painter, const QString& avatarPath, const QRect& avatarRect)
{
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QPixmap avatar = ImageService::instance().circularAvatarPreview(avatarPath,
                                                                          avatarRect.width(),
                                                                          dpr);
    if (avatar.isNull()) {
        MediaPlaceholderRenderer::drawAvatar(painter, avatarRect);
        return;
    }
    painter->drawPixmap(avatarRect, avatar);
}

InWindowPopupOverlay* overlayFor(QWidget* widget)
{
    for (QWidget* cursor = widget; cursor; cursor = cursor->parentWidget()) {
        if (auto* overlay = qobject_cast<InWindowPopupOverlay*>(cursor)) {
            return overlay;
        }
    }
    return nullptr;
}

} // namespace

CreateGroupChatContactModel::CreateGroupChatContactModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CreateGroupChatContactModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant CreateGroupChatContactModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const RowEntry& row = m_rows.at(index.row());
    if (row.groupIndex < 0 || row.groupIndex >= m_groups.size()) {
        return {};
    }

    const ContactGroup& group = m_groups.at(row.groupIndex);
    if (row.isGroup) {
        switch (role) {
        case Qt::DisplayRole:
        case GroupNameRole:
            return group.groupName;
        case IsGroupRole:
            return true;
        case GroupIdRole:
            return group.groupId;
        case GroupFriendCountRole:
            return group.totalCount;
        case GroupExpandedRole:
            return group.expanded;
        case GroupProgressRole:
            return group.progress;
        case Qt::SizeHintRole:
            return QSize(0, kGroupHeaderHeight);
        default:
            return {};
        }
    }

    if (row.friendIndex < 0 || row.friendIndex >= group.friends.size()) {
        return {};
    }

    const FriendSummary& contact = group.friends.at(row.friendIndex);
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return contact.displayName;
    case UserIdRole:
        return contact.userId;
    case AvatarPathRole:
        return contact.avatarPath;
    case StatusRole:
        return static_cast<int>(contact.status);
    case SignatureRole:
        return contact.signature;
    case NickNameRole:
        return contact.nickName;
    case RemarkRole:
        return contact.remark;
    case IsGroupRole:
        return false;
    case GroupIdRole:
        return group.groupId;
    case GroupNameRole:
        return group.groupName;
    case GroupProgressRole:
        return group.progress;
    case SelectedRole:
        return m_selectedUserIds.contains(contact.userId);
    case DisabledRole:
        return m_disabledUserIds.contains(contact.userId);
    case Qt::SizeHintRole:
        return QSize(0, qRound(kContactItemHeight * group.progress));
    default:
        return {};
    }
}

Qt::ItemFlags CreateGroupChatContactModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    if (isGroupRow(index)) {
        return Qt::ItemIsEnabled;
    }
    return index.data(GroupProgressRole).toReal() <= 0.01
            ? Qt::NoItemFlags
            : Qt::ItemIsEnabled;
}

void CreateGroupChatContactModel::reload(const QString& keyword,
                                         const QVector<FriendSummary>& recentContacts,
                                         const QVector<CreateGroupChatSourceGroup>& friendGroups,
                                         const QSet<QString>& selectedUserIds,
                                         const QSet<QString>& disabledUserIds)
{
    QHash<QString, bool> expandedByGroup;
    QHash<QString, qreal> progressByGroup;
    for (const ContactGroup& group : std::as_const(m_groups)) {
        expandedByGroup.insert(group.groupId, group.expanded);
        progressByGroup.insert(group.groupId, group.progress);
    }

    QVector<FriendSummary> filteredRecentContacts;
    filteredRecentContacts.reserve(recentContacts.size());
    QSet<QString> recentSeen;
    for (const FriendSummary& contact : recentContacts) {
        if (contact.userId.isEmpty() || recentSeen.contains(contact.userId) ||
            !matchesFriend(contact, keyword)) {
            continue;
        }
        filteredRecentContacts.push_back(contact);
        recentSeen.insert(contact.userId);
    }

    beginResetModel();
    m_selectedUserIds = selectedUserIds;
    m_disabledUserIds = disabledUserIds;
    m_groups.clear();

    ContactGroup recentGroup;
    recentGroup.groupId = QString::fromLatin1(kRecentGroupId);
    recentGroup.groupName = QStringLiteral("最近聊天");
    recentGroup.friends = filteredRecentContacts;
    recentGroup.totalCount = filteredRecentContacts.size();
    recentGroup.expanded = expandedByGroup.value(recentGroup.groupId, true);
    recentGroup.progress = progressByGroup.value(recentGroup.groupId, recentGroup.expanded ? 1.0 : 0.0);
    m_groups.push_back(recentGroup);

    for (const CreateGroupChatSourceGroup& sourceGroup : friendGroups) {
        QVector<FriendSummary> filtered;
        filtered.reserve(sourceGroup.friends.size());
        for (const FriendSummary& contact : sourceGroup.friends) {
            if (matchesFriend(contact, keyword)) {
                filtered.push_back(contact);
            }
        }
        if (filtered.isEmpty()) {
            continue;
        }

        ContactGroup group;
        group.groupId = sourceGroup.groupId.isEmpty() ? QStringLiteral("default") : sourceGroup.groupId;
        group.groupName = sourceGroup.groupName.isEmpty() ? QStringLiteral("默认分组") : sourceGroup.groupName;
        group.friends = std::move(filtered);
        group.totalCount = group.friends.size();
        group.expanded = expandedByGroup.value(group.groupId, false);
        group.progress = progressByGroup.value(group.groupId, group.expanded ? 1.0 : 0.0);
        m_groups.push_back(group);
    }

    rebuildRows();
    endResetModel();
}

void CreateGroupChatContactModel::reloadContacts(const QString& keyword,
                                                 const QVector<FriendSummary>& contacts,
                                                 const QString& groupId,
                                                 const QString& groupName,
                                                 const QSet<QString>& selectedUserIds,
                                                 const QSet<QString>& disabledUserIds)
{
    QHash<QString, bool> expandedByGroup;
    QHash<QString, qreal> progressByGroup;
    for (const ContactGroup& group : std::as_const(m_groups)) {
        expandedByGroup.insert(group.groupId, group.expanded);
        progressByGroup.insert(group.groupId, group.progress);
    }

    QVector<FriendSummary> filtered;
    filtered.reserve(contacts.size());
    for (const FriendSummary& contact : contacts) {
        if (matchesFriend(contact, keyword)) {
            filtered.push_back(contact);
        }
    }

    beginResetModel();
    m_selectedUserIds = selectedUserIds;
    m_disabledUserIds = disabledUserIds;
    m_groups.clear();

    ContactGroup group;
    group.groupId = groupId.isEmpty() ? QStringLiteral("group_members") : groupId;
    group.groupName = groupName.isEmpty() ? QStringLiteral("群聊成员") : groupName;
    group.friends = filtered;
    group.totalCount = filtered.size();
    group.expanded = expandedByGroup.value(group.groupId, true);
    group.progress = progressByGroup.value(group.groupId, group.expanded ? 1.0 : 0.0);
    m_groups.push_back(group);

    rebuildRows();
    endResetModel();
}

void CreateGroupChatContactModel::setSelectedUserIds(const QSet<QString>& selectedUserIds)
{
    if (m_selectedUserIds == selectedUserIds) {
        return;
    }
    const QSet<QString> previousSelectedUserIds = m_selectedUserIds;
    m_selectedUserIds = selectedUserIds;
    for (int rowIndex = 0; rowIndex < m_rows.size(); ++rowIndex) {
        const RowEntry& row = m_rows.at(rowIndex);
        if (row.isGroup || row.groupIndex < 0 || row.groupIndex >= m_groups.size()) {
            continue;
        }
        const ContactGroup& group = m_groups.at(row.groupIndex);
        if (row.friendIndex < 0 || row.friendIndex >= group.friends.size()) {
            continue;
        }
        const QString& userId = group.friends.at(row.friendIndex).userId;
        if (previousSelectedUserIds.contains(userId) != m_selectedUserIds.contains(userId)) {
            const QModelIndex changed = index(rowIndex, 0);
            emit dataChanged(changed, changed, {SelectedRole});
        }
    }
}

void CreateGroupChatContactModel::setGroupExpanded(const QString& groupId, bool expanded)
{
    ContactGroup* group = groupForId(groupId);
    if (!group || group->expanded == expanded) {
        return;
    }

    beginResetModel();
    group->expanded = expanded;
    rebuildRows();
    endResetModel();
}

void CreateGroupChatContactModel::setGroupProgress(const QString& groupId, qreal progress)
{
    ContactGroup* group = groupForId(groupId);
    if (!group) {
        return;
    }
    const qreal boundedProgress = qBound<qreal>(0.0, progress, 1.0);
    if (qFuzzyCompare(group->progress, boundedProgress)) {
        return;
    }
    group->progress = boundedProgress;
    const int firstRow = rowForGroup(groupId);
    const int lastRow = lastRowForGroup(groupId);
    if (firstRow >= 0 && lastRow >= firstRow) {
        emit dataChanged(index(firstRow, 0), index(lastRow, 0),
                         {GroupProgressRole, Qt::SizeHintRole});
    }
}

void CreateGroupChatContactModel::pruneCollapsedRows()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
}

bool CreateGroupChatContactModel::isGroupRow(const QModelIndex& index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
           m_rows.at(index.row()).isGroup;
}

bool CreateGroupChatContactModel::isContactRow(const QModelIndex& index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
           !m_rows.at(index.row()).isGroup;
}

QString CreateGroupChatContactModel::userIdAt(const QModelIndex& index) const
{
    return contactAt(index).userId;
}

FriendSummary CreateGroupChatContactModel::contactAt(const QModelIndex& index) const
{
    if (!isContactRow(index)) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_groups.at(row.groupIndex).friends.at(row.friendIndex);
}

QString CreateGroupChatContactModel::groupIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_groups.at(row.groupIndex).groupId;
}

bool CreateGroupChatContactModel::isContactDisabled(const QModelIndex& index) const
{
    return isContactRow(index) && m_disabledUserIds.contains(contactAt(index).userId);
}

int CreateGroupChatContactModel::groupRowForRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    const int groupIndex = m_rows.at(row).groupIndex;
    for (int candidate = row; candidate >= 0; --candidate) {
        const RowEntry& entry = m_rows.at(candidate);
        if (entry.isGroup && entry.groupIndex == groupIndex) {
            return candidate;
        }
    }
    return -1;
}

int CreateGroupChatContactModel::nextGroupRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    for (int candidate = row + 1; candidate < m_rows.size(); ++candidate) {
        if (m_rows.at(candidate).isGroup) {
            return candidate;
        }
    }
    return -1;
}

bool CreateGroupChatContactModel::isGroupExpanded(const QString& groupId) const
{
    const ContactGroup* group = groupForId(groupId);
    return group && group->expanded;
}

qreal CreateGroupChatContactModel::groupProgress(const QString& groupId) const
{
    const ContactGroup* group = groupForId(groupId);
    return group ? group->progress : 0.0;
}

CreateGroupChatContactModel::ContactGroup* CreateGroupChatContactModel::groupForId(const QString& groupId)
{
    for (ContactGroup& group : m_groups) {
        if (group.groupId == groupId) {
            return &group;
        }
    }
    return nullptr;
}

const CreateGroupChatContactModel::ContactGroup* CreateGroupChatContactModel::groupForId(const QString& groupId) const
{
    for (const ContactGroup& group : m_groups) {
        if (group.groupId == groupId) {
            return &group;
        }
    }
    return nullptr;
}

void CreateGroupChatContactModel::rebuildRows()
{
    m_rows.clear();
    for (int groupIndex = 0; groupIndex < m_groups.size(); ++groupIndex) {
        m_rows.push_back(RowEntry{true, groupIndex, -1});
        const ContactGroup& group = m_groups.at(groupIndex);
        if (!group.expanded && group.progress <= 0.01) {
            continue;
        }
        for (int friendIndex = 0; friendIndex < group.friends.size(); ++friendIndex) {
            m_rows.push_back(RowEntry{false, groupIndex, friendIndex});
        }
    }
}

int CreateGroupChatContactModel::rowForGroup(const QString& groupId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const RowEntry& entry = m_rows.at(row);
        if (entry.isGroup && entry.groupIndex >= 0 && entry.groupIndex < m_groups.size() &&
            m_groups.at(entry.groupIndex).groupId == groupId) {
            return row;
        }
    }
    return -1;
}

int CreateGroupChatContactModel::lastRowForGroup(const QString& groupId) const
{
    const int groupRow = rowForGroup(groupId);
    if (groupRow < 0) {
        return -1;
    }

    for (int row = groupRow + 1; row < m_rows.size(); ++row) {
        if (m_rows.at(row).isGroup) {
            return row - 1;
        }
    }
    return m_rows.size() - 1;
}

CreateGroupChatContactDelegate::CreateGroupChatContactDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        m_themePaintCache.valid = false;
    });
}

void CreateGroupChatContactDelegate::paint(QPainter* painter,
                                           const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const
{
    painter->save();
    painter->setClipRect(option.rect);
    AppFonts::configurePainterForText(*painter);

    const ThemePaintCache& theme = themePaintCache();
    const bool isGroup = index.data(CreateGroupChatContactModel::IsGroupRole).toBool();

    if (option.state & QStyle::State_MouseOver) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(theme.popupHover);
        painter->drawRoundedRect(option.rect.adjusted(6, 3, -6, -3), 6, 6);
    }

    if (isGroup) {
        const qreal progress = index.data(CreateGroupChatContactModel::GroupProgressRole).toReal();
        const QRect arrowRect(option.rect.left() + 13,
                              option.rect.top() + (option.rect.height() - kGroupArrowSize) / 2,
                              kGroupArrowSize,
                              kGroupArrowSize);
        drawGroupArrow(painter, arrowRect, progress);

        const QString title = index.data(CreateGroupChatContactModel::GroupNameRole).toString();
        const QString count = QString::number(index.data(CreateGroupChatContactModel::GroupFriendCountRole).toInt());
        const int titleLeft = arrowRect.right() + 8;
        const int countWidth = textMetrics(kGroupCountFontSize, QFont::Medium).horizontalAdvance(count);
        const int countRight = option.rect.right() - kGroupCountRightPadding;
        const QRect titleRect(titleLeft,
                              option.rect.top(),
                              qMax(0, countRight - countWidth - titleLeft - 8),
                              option.rect.height());
        const QRect countRect(countRight - countWidth,
                              option.rect.top(),
                              countWidth,
                              option.rect.height());
        painter->setFont(textFont(kGroupTitleFontSize, QFont::Medium));
        painter->setPen(theme.secondaryText);
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          textMetrics(kGroupTitleFontSize, QFont::Medium)
                                  .elidedText(title, Qt::ElideRight, titleRect.width()));
        painter->setFont(textFont(kGroupCountFontSize, QFont::Medium));
        painter->setPen(theme.tertiaryText);
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, count);
        painter->restore();
        return;
    }

    const qreal progress = index.data(CreateGroupChatContactModel::GroupProgressRole).toReal();
    if (progress <= 0.01 || option.rect.height() <= 0) {
        painter->restore();
        return;
    }

    painter->setOpacity(qMin(1.0, progress * 1.25));
    const bool disabled = index.data(CreateGroupChatContactModel::DisabledRole).toBool();
    if (disabled) {
        painter->setOpacity(painter->opacity() * 0.42);
    }
    const ContactPaintCache contact = contactPaintCache(index);
    const int contentTop = option.rect.top() - qRound((1.0 - progress) * 10.0);
    const QRect circleRect(option.rect.left() + kLeftListCircleLeft,
                           contentTop + (kContactItemHeight - kSelectCircleSize) / 2,
                           kSelectCircleSize,
                           kSelectCircleSize);
    drawSelectionCircle(painter, circleRect,
                        index.data(CreateGroupChatContactModel::SelectedRole).toBool(),
                        disabled);

    const QRect avatarRect(circleRect.right() + 12,
                           contentTop + (kContactItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    drawAvatar(painter, contact.avatarPath, avatarRect);

    const int textLeft = avatarRect.right() + 9;
    const int textRight = option.rect.right() - 16;
    const QRect nameRect(textLeft, contentTop, qMax(0, textRight - textLeft), kContactItemHeight);

    painter->setFont(textFont(14));
    painter->setPen(theme.primaryText);
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      textMetrics(14).elidedText(contact.displayName, Qt::ElideRight, nameRect.width()));

    painter->restore();
}

QSize CreateGroupChatContactDelegate::sizeHint(const QStyleOptionViewItem& option,
                                               const QModelIndex& index) const
{
    Q_UNUSED(option);
    return index.data(CreateGroupChatContactModel::IsGroupRole).toBool()
            ? QSize(0, kGroupHeaderHeight)
            : QSize(0, index.data(Qt::SizeHintRole).toSize().height());
}

void CreateGroupChatContactDelegate::clearPaintCache()
{
    m_contactPaintCache.clear();
    m_themePaintCache.valid = false;
}

void CreateGroupChatContactDelegate::invalidatePaintCache(const QModelIndex& topLeft,
                                                          const QModelIndex& bottomRight,
                                                          const QVector<int>& roles)
{
    if (!topLeft.isValid() || !bottomRight.isValid() || topLeft.model() != bottomRight.model()) {
        return;
    }

    if (!roles.isEmpty()) {
        bool affectsContactPaint = false;
        for (const int role : roles) {
            switch (role) {
            case Qt::DisplayRole:
            case CreateGroupChatContactModel::DisplayNameRole:
            case CreateGroupChatContactModel::AvatarPathRole:
                affectsContactPaint = true;
                break;
            default:
                break;
            }
            if (affectsContactPaint) {
                break;
            }
        }
        if (!affectsContactPaint) {
            return;
        }
    }

    for (int row = topLeft.row(); row <= bottomRight.row(); ++row) {
        const QModelIndex modelIndex = topLeft.sibling(row, topLeft.column());
        const QString userId = modelIndex.data(CreateGroupChatContactModel::UserIdRole).toString();
        if (!userId.isEmpty()) {
            m_contactPaintCache.remove(userId);
        }
    }
}

const CreateGroupChatContactDelegate::ThemePaintCache&
CreateGroupChatContactDelegate::themePaintCache() const
{
    const bool dark = ThemeManager::instance().isDark();
    if (m_themePaintCache.valid && m_themePaintCache.dark == dark) {
        return m_themePaintCache;
    }

    m_themePaintCache.valid = true;
    m_themePaintCache.dark = dark;
    m_themePaintCache.popupHover = ThemeManager::instance().color(ThemeColor::CreateGroupPopupHover);
    m_themePaintCache.primaryText = ThemeManager::instance().color(ThemeColor::CreateGroupPopupPrimaryText);
    m_themePaintCache.secondaryText = ThemeManager::instance().color(ThemeColor::CreateGroupPopupSecondaryText);
    m_themePaintCache.tertiaryText = ThemeManager::instance().color(ThemeColor::CreateGroupPopupTertiaryText);
    return m_themePaintCache;
}

CreateGroupChatContactDelegate::ContactPaintCache
CreateGroupChatContactDelegate::contactPaintCache(const QModelIndex& index) const
{
    const QString userId = index.data(CreateGroupChatContactModel::UserIdRole).toString();
    if (!userId.isEmpty()) {
        const auto cached = m_contactPaintCache.constFind(userId);
        if (cached != m_contactPaintCache.cend()) {
            return cached.value();
        }
    }

    ContactPaintCache data;
    data.userId = userId;
    data.displayName = index.data(CreateGroupChatContactModel::DisplayNameRole).toString();
    data.avatarPath = index.data(CreateGroupChatContactModel::AvatarPathRole).toString();

    if (!data.userId.isEmpty()) {
        m_contactPaintCache.insert(data.userId, data);
    }
    return data;
}

CreateGroupChatContactListView::CreateGroupChatContactListView(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new CreateGroupChatContactModel(this))
    , m_delegate(new CreateGroupChatContactDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::NoSelection);
    setUniformItemSizes(false);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    makeListViewTransparent(this);
    setThemeBackgroundRole(ThemeColor::CreateGroupPopupBackground);
    refreshTheme();
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    connect(&ImageService::instance(), &ImageService::previewReady,
            viewport(), QOverload<>::of(&QWidget::update));
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, &CreateGroupChatContactListView::updateStickyHeader);
    connect(m_model, &QAbstractItemModel::modelReset, this, [this]() {
        m_delegate->clearPaintCache();
        updateStickyHeader();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, [this](const QModelIndex&, int, int) {
        m_delegate->clearPaintCache();
        updateStickyHeader();
    });
    connect(m_model, &QAbstractItemModel::rowsRemoved, this, [this](const QModelIndex&, int, int) {
        m_delegate->clearPaintCache();
        updateStickyHeader();
    });
    connect(m_model, &QAbstractItemModel::dataChanged, this, [this](const QModelIndex& topLeft,
                                                                    const QModelIndex& bottomRight,
                                                                    const QVector<int>& roles) {
        m_delegate->invalidatePaintCache(topLeft, bottomRight, roles);
        updateStickyHeader();
    });
}

void CreateGroupChatContactListView::reload(const QString& keyword,
                                            const QVector<FriendSummary>& recentContacts,
                                            const QVector<CreateGroupChatSourceGroup>& friendGroups,
                                            const QSet<QString>& selectedUserIds,
                                            const QSet<QString>& disabledUserIds)
{
    m_model->reload(keyword, recentContacts, friendGroups, selectedUserIds, disabledUserIds);
}

void CreateGroupChatContactListView::reloadContacts(const QString& keyword,
                                                    const QVector<FriendSummary>& contacts,
                                                    const QString& groupId,
                                                    const QString& groupName,
                                                    const QSet<QString>& selectedUserIds,
                                                    const QSet<QString>& disabledUserIds)
{
    m_model->reloadContacts(keyword, contacts, groupId, groupName, selectedUserIds, disabledUserIds);
}

void CreateGroupChatContactListView::setSelectedUserIds(const QSet<QString>& selectedUserIds)
{
    m_model->setSelectedUserIds(selectedUserIds);
    viewport()->update();
}

void CreateGroupChatContactListView::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);
    drawStickyHeader();
}

void CreateGroupChatContactListView::leaveEvent(QEvent* event)
{
    OverlayScrollListView::leaveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, m_stickyOffsetY, viewport()->width(), kGroupHeaderHeight));
    }
}

void CreateGroupChatContactListView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QRect stickyRect(0, m_stickyOffsetY, viewport()->width(), kGroupHeaderHeight);
    if (m_stickyVisible && !m_stickyGroup.groupId.isEmpty() && stickyRect.contains(event->pos())) {
        toggleStickyGroupById(m_stickyGroup.groupId);
        event->accept();
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        event->accept();
        return;
    }

    if (m_model->isGroupRow(index)) {
        toggleGroup(m_model->groupIdAt(index));
        event->accept();
        return;
    }

    if (m_model->isContactRow(index)) {
        if (m_model->isContactDisabled(index)) {
            event->accept();
            return;
        }
        emit contactToggled(m_model->contactAt(index));
        event->accept();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
}

void CreateGroupChatContactListView::mouseMoveEvent(QMouseEvent* event)
{
    OverlayScrollListView::mouseMoveEvent(event);
    if (m_stickyVisible) {
        viewport()->update(QRect(0, m_stickyOffsetY, viewport()->width(), kGroupHeaderHeight));
    }
}

void CreateGroupChatContactListView::toggleGroup(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return;
    }
    setGroupExpandedAnimated(groupId, !m_model->isGroupExpanded(groupId));
}

void CreateGroupChatContactListView::toggleStickyGroupById(const QString& groupId)
{
    if (groupId.isEmpty()) {
        return;
    }
    setGroupExpandedAnimated(groupId, !m_model->isGroupExpanded(groupId));
}

void CreateGroupChatContactListView::setGroupExpandedAnimated(const QString& groupId, bool expanded)
{
    const qreal start = m_model->groupProgress(groupId);
    const qreal end = expanded ? 1.0 : 0.0;
    if (m_model->isGroupExpanded(groupId) == expanded && qAbs(start - end) <= 0.001) {
        return;
    }

    if (QPointer<QVariantAnimation> running = m_groupAnimations.value(groupId)) {
        running->stop();
        running->deleteLater();
    }

    m_model->setGroupExpanded(groupId, expanded);
    auto* animation = new QVariantAnimation(this);
    m_groupAnimations.insert(groupId, animation);
    animation->setStartValue(start);
    animation->setEndValue(end);
    animation->setDuration(420);
    animation->setEasingCurve(QEasingCurve::OutCubic);

    connect(animation, &QVariantAnimation::valueChanged, this, [this, groupId](const QVariant& value) {
        m_model->setGroupProgress(groupId, value.toReal());
        doItemsLayout();
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });
    connect(animation, &QVariantAnimation::finished, this, [this, groupId, end, animation]() {
        m_model->setGroupProgress(groupId, end);
        if (end <= 0.01) {
            m_model->pruneCollapsedRows();
        }
        m_groupAnimations.remove(groupId);
        animation->deleteLater();
        doItemsLayout();
        viewport()->update();
        updateOverlayScrollBar();
        updateStickyHeader();
    });
    animation->start();
}

void CreateGroupChatContactListView::updateStickyHeader()
{
    const StickyHeaderState state = calculateStickyHeaderState();
    m_stickyVisible = state.visible;
    m_stickyOffsetY = state.offsetY;

    if (!m_stickyVisible) {
        m_stickyGroup = {};
        viewport()->update();
        return;
    }

    m_stickyGroup = state.group;
    viewport()->update();
}

CreateGroupChatContactListView::StickyHeaderState
CreateGroupChatContactListView::calculateStickyHeaderState() const
{
    StickyHeaderState state;
    if (m_model->rowCount() <= 0 || verticalScrollBar()->value() <= 0) {
        return state;
    }

    QModelIndex firstVisible = indexAt(QPoint(1, 1));
    if (!firstVisible.isValid()) {
        firstVisible = m_model->index(0, 0);
    }
    if (!firstVisible.isValid()) {
        return state;
    }

    const int currentGroupRow = m_model->groupRowForRow(firstVisible.row());
    if (currentGroupRow < 0) {
        return state;
    }

    state.visible = true;
    state.group = stickyGroupDataForRow(currentGroupRow);

    const int nextGroupRow = m_model->nextGroupRow(currentGroupRow);
    if (nextGroupRow >= 0) {
        const QRect nextRect = visualRect(m_model->index(nextGroupRow, 0));
        if (nextRect.isValid()) {
            state.offsetY = qMin(0, nextRect.top() - kGroupHeaderHeight);
        }
    }

    return state;
}

CreateGroupChatContactListView::StickyGroupData
CreateGroupChatContactListView::stickyGroupDataForRow(int row) const
{
    const QModelIndex index = m_model->index(row, 0);
    if (!index.isValid()) {
        return {};
    }

    StickyGroupData group;
    group.groupId = index.data(CreateGroupChatContactModel::GroupIdRole).toString();
    group.title = index.data(CreateGroupChatContactModel::GroupNameRole).toString();
    group.count = index.data(CreateGroupChatContactModel::GroupFriendCountRole).toInt();
    group.progress = index.data(CreateGroupChatContactModel::GroupProgressRole).toReal();
    return group;
}

void CreateGroupChatContactListView::drawStickyHeader() const
{
    if (!m_stickyVisible || m_stickyGroup.groupId.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), kGroupHeaderHeight));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), kGroupHeaderHeight);
    painter.fillRect(headerRect, ThemeManager::instance().color(ThemeColor::CreateGroupPopupBackground));
    drawStickyGroup(&painter, headerRect, m_stickyGroup);
}

void CreateGroupChatContactListView::drawStickyGroup(QPainter* painter,
                                                     const QRect& rect,
                                                     const StickyGroupData& group) const
{
    if (group.groupId.isEmpty()) {
        return;
    }

    painter->save();
    if (rect.contains(viewport()->mapFromGlobal(QCursor::pos()))) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(ThemeManager::instance().color(ThemeColor::CreateGroupPopupHover));
        painter->drawRoundedRect(rect.adjusted(6, 3, -6, -3), 6, 6);
    }

    const QRect arrowRect(rect.left() + 13,
                          rect.top() + (rect.height() - kGroupArrowSize) / 2,
                          kGroupArrowSize,
                          kGroupArrowSize);
    drawGroupArrow(painter, arrowRect, group.progress);

    const QString countText = QString::number(group.count);
    const int titleLeft = arrowRect.right() + 8;
    const int countWidth = textMetrics(kGroupCountFontSize, QFont::Medium).horizontalAdvance(countText);
    const int countRight = rect.right() - kGroupCountRightPadding;
    const QRect countRect(qMax(titleLeft, countRight - countWidth + 1),
                          rect.top(),
                          countWidth,
                          rect.height());
    const QRect titleRect(titleLeft,
                          rect.top(),
                          qMax(0, countRect.left() - titleLeft - 8),
                          rect.height());

    painter->setFont(textFont(kGroupTitleFontSize, QFont::Medium));
    painter->setPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupSecondaryText));
    painter->drawText(titleRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      textMetrics(kGroupTitleFontSize, QFont::Medium)
                              .elidedText(group.title, Qt::ElideRight, titleRect.width()));

    painter->setFont(textFont(kGroupCountFontSize, QFont::Medium));
    painter->setPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupTertiaryText));
    painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    painter->restore();
}

CreateGroupChatSelectedModel::CreateGroupChatSelectedModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int CreateGroupChatSelectedModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : m_contacts.size();
}

QVariant CreateGroupChatSelectedModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contacts.size()) {
        return {};
    }

    const FriendSummary& contact = m_contacts.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return contact.displayName;
    case UserIdRole:
        return contact.userId;
    case AvatarPathRole:
        return contact.avatarPath;
    case SignatureRole:
        return contact.signature;
    case Qt::SizeHintRole:
        return QSize(0, kSelectedItemHeight);
    default:
        return {};
    }
}

Qt::ItemFlags CreateGroupChatSelectedModel::flags(const QModelIndex& index) const
{
    return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
}

void CreateGroupChatSelectedModel::setContacts(QVector<FriendSummary> contacts)
{
    beginResetModel();
    m_contacts = std::move(contacts);
    endResetModel();
}

FriendSummary CreateGroupChatSelectedModel::contactAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_contacts.size()) {
        return {};
    }
    return m_contacts.at(index.row());
}

CreateGroupChatSelectedDelegate::CreateGroupChatSelectedDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void CreateGroupChatSelectedDelegate::paint(QPainter* painter,
                                            const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    painter->save();
    AppFonts::configurePainterForText(*painter);
    painter->setClipRect(option.rect);
    if (option.state & QStyle::State_MouseOver) {
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(Qt::NoPen);
        painter->setBrush(ThemeManager::instance().color(ThemeColor::CreateGroupPopupHover));
        painter->drawRoundedRect(option.rect.adjusted(6, 4, -6, -4), 6, 6);
    }

    const QRect avatarRect(option.rect.left() + 14,
                           option.rect.top() + (option.rect.height() - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    drawAvatar(painter, index.data(CreateGroupChatSelectedModel::AvatarPathRole).toString(), avatarRect);

    const QRect removeRect = removeButtonRect(option.rect);
    const int textLeft = avatarRect.right() + 10;
    const int textRight = removeRect.left() - 10;
    const QRect nameRect(textLeft, option.rect.top(), qMax(0, textRight - textLeft), option.rect.height());

    painter->setFont(textFont(14));
    painter->setPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupPrimaryText));
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      textMetrics(14).elidedText(index.data(CreateGroupChatSelectedModel::DisplayNameRole).toString(),
                                                 Qt::ElideRight,
                                                 nameRect.width()));

    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::CreateGroupPopupDivider));
    const QRectF removeEllipseRect(removeRect);
    const QPointF removeCenter = removeEllipseRect.center();
    painter->drawEllipse(removeEllipseRect);
    painter->setPen(QPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupSecondaryText), 1.35,
                         Qt::SolidLine,
                         Qt::RoundCap));
    constexpr qreal crossHalf = 3.0;
    painter->drawLine(QPointF(removeCenter.x() - crossHalf, removeCenter.y() - crossHalf),
                      QPointF(removeCenter.x() + crossHalf, removeCenter.y() + crossHalf));
    painter->drawLine(QPointF(removeCenter.x() + crossHalf, removeCenter.y() - crossHalf),
                      QPointF(removeCenter.x() - crossHalf, removeCenter.y() + crossHalf));
    painter->restore();
}

QSize CreateGroupChatSelectedDelegate::sizeHint(const QStyleOptionViewItem& option,
                                                const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, kSelectedItemHeight);
}

QRect CreateGroupChatSelectedDelegate::removeButtonRect(const QRect& itemRect)
{
    constexpr int size = 18;
    return QRect(itemRect.right() - 18 - size,
                 itemRect.top() + (itemRect.height() - size) / 2,
                 size,
                 size);
}

CreateGroupChatSelectedListView::CreateGroupChatSelectedListView(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new CreateGroupChatSelectedModel(this))
    , m_delegate(new CreateGroupChatSelectedDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::NoSelection);
    setUniformItemSizes(true);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    makeListViewTransparent(this);
    setThemeBackgroundRole(ThemeColor::CreateGroupPopupBackground);
    refreshTheme();
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);
    connect(&ImageService::instance(), &ImageService::previewReady,
            viewport(), QOverload<>::of(&QWidget::update));
}

void CreateGroupChatSelectedListView::setContacts(QVector<FriendSummary> contacts)
{
    m_model->setContacts(std::move(contacts));
}

void CreateGroupChatSelectedListView::paintEvent(QPaintEvent* event)
{
    QListView::paintEvent(event);
}

void CreateGroupChatSelectedListView::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        OverlayScrollListView::mousePressEvent(event);
        return;
    }

    const QModelIndex index = indexAt(event->pos());
    if (!index.isValid()) {
        event->accept();
        return;
    }

    if (CreateGroupChatSelectedDelegate::removeButtonRect(visualRect(index)).contains(event->pos())) {
        emit removeRequested(m_model->contactAt(index).userId);
        event->accept();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
}

CreateGroupChatPopup::CreateGroupChatPopup(QWidget* parent)
    : QWidget(parent)
    , m_searchInput(new CreateGroupChatSearchInput(this))
    , m_contactList(new CreateGroupChatContactListView(this))
    , m_selectedList(new CreateGroupChatSelectedListView(this))
    , m_cancelButton(new StatefulPushButton(QStringLiteral("取消"), this))
    , m_okButton(new StatefulPushButton(QStringLiteral("确定"), this))
{
    setMinimumSize(kPopupWidth, kPopupHeight);
    setFocusProxy(m_searchInput);

    m_searchInput->setFixedHeight(kSearchHeight);
    m_cancelButton->setFixedSize(82, 32);
    m_okButton->setFixedSize(82, 32);

    applyTheme();
    reloadContacts();
    refreshSelectedView();

    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
        update();
    });
    connect(m_searchInput, &QLineEdit::textChanged, this, [this]() {
        reloadContacts();
    });
    connect(m_contactList, &CreateGroupChatContactListView::contactToggled,
            this, &CreateGroupChatPopup::toggleContact);
    connect(m_selectedList, &CreateGroupChatSelectedListView::removeRequested,
            this, &CreateGroupChatPopup::removeContact);
    connect(m_cancelButton, &QPushButton::clicked, this, [this]() {
        emit rejected();
        if (auto* overlay = overlayFor(this)) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Rejected);
        }
    });
    connect(m_okButton, &QPushButton::clicked, this, [this]() {
        if (m_useCustomContacts || m_title != QStringLiteral("创建群聊")) {
            acceptSelection();
            return;
        }
        createGroup();
    });

    QTimer::singleShot(0, m_searchInput, [this]() {
        m_searchInput->setFocus(Qt::PopupFocusReason);
    });
}

QString CreateGroupChatPopup::open(QWidget* parent)
{
    QString result;
    auto* content = new CreateGroupChatPopup;

    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(kPopupWidth, kPopupHeight);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        return {};
    }

    QObject::connect(content, &CreateGroupChatPopup::accepted, content, [&result, overlay](const QString& groupId) {
        result = groupId;
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    });

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

QStringList CreateGroupChatPopup::openInviteMembers(QWidget* parent, const Group& group)
{
    auto* content = new CreateGroupChatPopup;
    content->configureForInvite(group, true);
    return openSelectionPopup(parent, content);
}

QStringList CreateGroupChatPopup::openPreviewInviteMembers(QWidget* parent, const Group& group)
{
    auto* content = new CreateGroupChatPopup;
    content->configureForInvite(group, false);
    return openSelectionPopup(parent, content);
}

QStringList CreateGroupChatPopup::openRemoveMembers(QWidget* parent, const Group& group)
{
    auto* content = new CreateGroupChatPopup;
    content->configureForRemove(group);
    return openSelectionPopup(parent, content);
}

QString CreateGroupChatPopup::openTransferOwner(QWidget* parent, const Group& group)
{
    auto* content = new CreateGroupChatPopup;
    content->configureForTransferOwner(group);
    const QStringList result = openSelectionPopup(parent, content);
    return result.isEmpty() ? QString() : result.first();
}

QStringList CreateGroupChatPopup::openSelectionPopup(QWidget* parent, CreateGroupChatPopup* content)
{
    QStringList result;
    if (!content) {
        return result;
    }

    InWindowPopupOverlay::Options options;
    options.maximumPopupSize = QSize(kPopupWidth, kPopupHeight);
    options.dismissOnOutsideClick = true;
    options.dismissOnEscape = true;

    QPointer<InWindowPopupOverlay> overlay = InWindowPopupOverlay::showPopup(parent, content, options);
    if (!overlay) {
        content->deleteLater();
        return result;
    }

    QObject::connect(content, &CreateGroupChatPopup::selectionAccepted, content, [&result, overlay](const QStringList& userIds) {
        result = userIds;
        if (overlay) {
            overlay->closePopup(InWindowPopupOverlay::DismissReason::Accepted);
        }
    });

    QEventLoop loop;
    QObject::connect(overlay, &InWindowPopupOverlay::dismissed, &loop, &QEventLoop::quit);
    loop.exec();
    return result;
}

void CreateGroupChatPopup::paintEvent(QPaintEvent* event)
{
    Q_UNUSED(event);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(painter);

    const QRectF backgroundRect = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    QPainterPath popupPath;
    popupPath.addRoundedRect(backgroundRect, kPopupRadius, kPopupRadius);
    painter.setClipPath(popupPath);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ThemeManager::instance().color(ThemeColor::CreateGroupPopupBackground));
    painter.drawPath(popupPath);

    const int dividerX = kPadding + kLeftPaneWidth + 1;
    painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupDivider), 1));
    painter.drawLine(dividerX, 0, dividerX, height());

    const QRect rightTitleRect(dividerX + 16, kPadding + 1, 170, 24);
    const QRect countRect(width() - kPadding - 170, kPadding + 2, 170, 26);

    painter.setFont(textFont(16, QFont::DemiBold));
    painter.setPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupPrimaryText));
    painter.drawText(rightTitleRect, Qt::AlignLeft | Qt::AlignVCenter, m_title);

    painter.setFont(textFont(13));
    painter.setPen(ThemeManager::instance().color(ThemeColor::CreateGroupPopupSecondaryText));
    painter.drawText(countRect,
                     Qt::AlignRight | Qt::AlignVCenter,
                     QStringLiteral("已选择%1个%2").arg(m_selectedContacts.size()).arg(m_countUnit));
}

void CreateGroupChatPopup::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    const int dividerX = kPadding + kLeftPaneWidth + 1;
    const int footerTop = height() - kFooterHeight;
    m_searchInput->setGeometry(kPadding,
                               kPadding,
                               qMax(0, dividerX - kPadding * 2),
                               kSearchHeight);
    m_contactList->setGeometry(0,
                               kPadding + kSearchHeight + 8,
                               dividerX,
                               qMax(0, height() - kPadding - kSearchHeight - 8));

    const int rightLeft = dividerX + 14;
    const int rightTop = kPadding + 36;
    m_selectedList->setGeometry(rightLeft,
                                rightTop,
                                qMax(0, width() - rightLeft - kPadding),
                                qMax(0, footerTop - rightTop));

    m_okButton->move(width() - kPadding - m_okButton->width(),
                     footerTop + (kFooterHeight - m_okButton->height()) / 2);
    m_cancelButton->move(m_okButton->x() - 10 - m_cancelButton->width(),
                         m_okButton->y());
}

void CreateGroupChatPopup::applyTheme()
{
    m_cancelButton->setDefaultStyle();
    m_cancelButton->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    m_cancelButton->setBorderWidth(1);
    m_okButton->setPrimaryStyle();
    m_okButton->setEnabled(!m_selectedContacts.isEmpty());
}

void CreateGroupChatPopup::reloadContacts()
{
    if (m_useCustomContacts) {
        m_contactList->reloadContacts(m_searchInput->text().trimmed(),
                                      m_sourceContacts,
                                      m_sourceGroupId,
                                      m_sourceGroupName,
                                      m_selectedUserIds,
                                      m_disabledUserIds);
        return;
    }

    ensureContactCache();
    m_contactList->reload(m_searchInput->text().trimmed(),
                          m_recentContacts,
                          m_friendGroups,
                          m_selectedUserIds,
                          m_disabledUserIds);
}

void CreateGroupChatPopup::ensureContactCache()
{
    if (m_contactCacheLoaded) {
        return;
    }
    m_contactCacheLoaded = true;
    m_recentContacts.clear();
    m_friendGroups.clear();

    const QVector<FriendSummary> allFriends = UserRepository::instance().requestFriendList();
    QHash<QString, FriendSummary> friendById;
    friendById.reserve(allFriends.size());
    QHash<QString, int> groupIndexById;

    for (const FriendSummary& contact : allFriends) {
        if (contact.userId.isEmpty()) {
            continue;
        }

        friendById.insert(contact.userId, contact);
        const QString groupId = contact.groupId.isEmpty() ? QStringLiteral("default") : contact.groupId;
        int groupIndex = groupIndexById.value(groupId, -1);
        if (groupIndex < 0) {
            CreateGroupChatSourceGroup group;
            group.groupId = groupId;
            group.groupName = contact.groupName.isEmpty() ? QStringLiteral("默认分组") : contact.groupName;
            m_friendGroups.push_back(group);
            groupIndex = m_friendGroups.size() - 1;
            groupIndexById.insert(groupId, groupIndex);
        }
        m_friendGroups[groupIndex].friends.push_back(contact);
    }

    const QVector<ConversationSummary> conversations =
            MessageRepository::instance().requestConversationList();
    QStringList missingUserIds;
    QSet<QString> missingSeen;
    for (const ConversationSummary& conversation : conversations) {
        if (conversation.isGroup || conversation.conversationId.isEmpty() ||
            friendById.contains(conversation.conversationId) ||
            missingSeen.contains(conversation.conversationId)) {
            continue;
        }
        missingSeen.insert(conversation.conversationId);
        missingUserIds.push_back(conversation.conversationId);
    }

    QHash<QString, FriendSummary> userContactById;
    for (const User& user : UserRepository::instance().requestUserDetails(missingUserIds)) {
        const FriendSummary contact = friendSummaryFromUser(user);
        if (!contact.userId.isEmpty()) {
            userContactById.insert(contact.userId, contact);
        }
    }

    QSet<QString> recentSeen;
    for (const ConversationSummary& conversation : conversations) {
        if (conversation.isGroup || conversation.conversationId.isEmpty() ||
            recentSeen.contains(conversation.conversationId)) {
            continue;
        }

        FriendSummary contact = friendById.value(conversation.conversationId);
        if (contact.userId.isEmpty()) {
            contact = userContactById.value(conversation.conversationId);
        }
        if (contact.userId.isEmpty()) {
            continue;
        }
        m_recentContacts.push_back(contact);
        recentSeen.insert(contact.userId);
    }
}

void CreateGroupChatPopup::toggleContact(const FriendSummary& contact)
{
    if (contact.userId.isEmpty() || m_disabledUserIds.contains(contact.userId)) {
        return;
    }

    if (m_selectedUserIds.contains(contact.userId)) {
        removeContact(contact.userId);
        return;
    }

    if (m_singleSelection) {
        m_selectedUserIds.clear();
        m_selectedContacts.clear();
    }
    m_selectedUserIds.insert(contact.userId);
    m_selectedContacts.push_back(contact);
    refreshSelectedView();
}

void CreateGroupChatPopup::removeContact(const QString& userId)
{
    if (userId.isEmpty() || !m_selectedUserIds.contains(userId)) {
        return;
    }

    m_selectedUserIds.remove(userId);
    for (int index = 0; index < m_selectedContacts.size(); ++index) {
        if (m_selectedContacts.at(index).userId == userId) {
            m_selectedContacts.removeAt(index);
            break;
        }
    }
    refreshSelectedView();
}

void CreateGroupChatPopup::refreshSelectedView()
{
    m_contactList->setSelectedUserIds(m_selectedUserIds);
    m_selectedList->setContacts(m_selectedContacts);
    m_okButton->setEnabled(!m_selectedContacts.isEmpty());
    update();
}

void CreateGroupChatPopup::createGroup()
{
    if (m_selectedContacts.isEmpty()) {
        return;
    }

    const CurrentUser& currentUser = CurrentUser::instance();
    Group group;
    group.groupId = QStringLiteral("g_custom_%1")
            .arg(QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (m_selectedContacts.size() == 1) {
        group.groupName = QStringLiteral("%1、%2").arg(currentUser.getUserName(),
                                                      m_selectedContacts.first().displayName);
    } else {
        group.groupName = QStringLiteral("%1等%2人群聊")
                .arg(m_selectedContacts.first().displayName)
                .arg(m_selectedContacts.size() + 1);
    }
    group.memberNum = m_selectedContacts.size() + 1;
    group.ownerId = currentUser.getUserId();
    group.groupAvatarPath = m_selectedContacts.first().avatarPath.isEmpty()
            ? QStringLiteral(":/resources/avatar/10.jpg")
            : m_selectedContacts.first().avatarPath;
    group.currentUserNickname = currentUser.getUserName();
    group.listGroupId = QStringLiteral("gg_joined");
    group.listGroupName = QStringLiteral("我加入的群聊");
    group.membersID.push_back(currentUser.getUserId());
    group.memberNicknames.insert(currentUser.getUserId(), currentUser.getUserName());

    for (const FriendSummary& contact : std::as_const(m_selectedContacts)) {
        if (contact.userId.isEmpty() || group.membersID.contains(contact.userId)) {
            continue;
        }
        group.membersID.push_back(contact.userId);
        group.memberNicknames.insert(contact.userId,
                                     contact.nickName.isEmpty() ? contact.displayName : contact.nickName);
    }
    group.memberNum = group.membersID.size();

    GroupRepository::instance().saveGroup(group);
    MessageRepository::instance().touchConversation(group.groupId, QDateTime::currentDateTime());
    emit accepted(group.groupId);
}

void CreateGroupChatPopup::acceptSelection()
{
    if (m_selectedUserIds.isEmpty()) {
        return;
    }

    if (!m_commitSelection) {
        emit selectionAccepted({});
        return;
    }

    QStringList userIds;
    userIds.reserve(m_selectedContacts.size());
    for (const FriendSummary& contact : std::as_const(m_selectedContacts)) {
        if (!contact.userId.isEmpty() && !m_disabledUserIds.contains(contact.userId)) {
            userIds.push_back(contact.userId);
        }
    }
    emit selectionAccepted(userIds);
}

void CreateGroupChatPopup::configureForInvite(const Group& group, bool commitEnabled)
{
    m_useCustomContacts = false;
    m_commitSelection = commitEnabled;
    m_singleSelection = false;
    m_disabledUserIds.clear();
    for (const QString& userId : group.membersID) {
        if (!userId.isEmpty()) {
            m_disabledUserIds.insert(userId);
        }
    }
    setModeTitle(QStringLiteral("邀请新成员"), QStringLiteral("好友"));
    m_okButton->setText(commitEnabled ? QStringLiteral("邀请") : QStringLiteral("确定"));
    m_selectedContacts.clear();
    m_selectedUserIds.clear();
    reloadContacts();
    refreshSelectedView();
}

void CreateGroupChatPopup::configureForRemove(const Group& group)
{
    m_useCustomContacts = true;
    m_commitSelection = true;
    m_singleSelection = false;
    m_sourceContacts = groupMembersAsContacts(group);
    m_sourceGroupId = group.groupId;
    m_sourceGroupName = QStringLiteral("群聊成员");
    m_disabledUserIds = removableDisabledUserIds(group);
    setModeTitle(QStringLiteral("移除成员"), QStringLiteral("成员"));
    m_okButton->setText(QStringLiteral("移除"));
    m_selectedContacts.clear();
    m_selectedUserIds.clear();
    reloadContacts();
    refreshSelectedView();
}

void CreateGroupChatPopup::configureForTransferOwner(const Group& group)
{
    m_useCustomContacts = true;
    m_commitSelection = true;
    m_singleSelection = true;
    m_sourceContacts = groupMembersAsContacts(group);
    m_sourceGroupId = group.groupId;
    m_sourceGroupName = QStringLiteral("选择新群主");
    m_disabledUserIds = {CurrentUser::instance().getUserId()};
    if (!group.ownerId.isEmpty()) {
        m_disabledUserIds.insert(group.ownerId);
    }
    setModeTitle(QStringLiteral("转让群聊"), QStringLiteral("成员"));
    m_okButton->setText(QStringLiteral("转让"));
    m_selectedContacts.clear();
    m_selectedUserIds.clear();
    reloadContacts();
    refreshSelectedView();
}

void CreateGroupChatPopup::setModeTitle(const QString& title, const QString& countUnit)
{
    m_title = title;
    m_countUnit = countUnit;
    update();
}
