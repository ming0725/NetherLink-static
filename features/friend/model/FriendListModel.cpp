#include "FriendListModel.h"

#include <QSize>

namespace {

constexpr int kGroupHeaderHeight = 36;
constexpr int kFriendItemHeight = 72;

QString normalizedGroupId(const FriendSummary& friendSummary)
{
    return friendSummary.groupId.isEmpty() ? QStringLiteral("default") : friendSummary.groupId;
}

QString normalizedGroupName(const FriendSummary& friendSummary)
{
    return friendSummary.groupName.isEmpty() ? QStringLiteral("默认分组") : friendSummary.groupName;
}

} // namespace

FriendListModel::FriendListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int FriendListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant FriendListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const RowEntry& row = m_rows.at(index.row());
    if (row.type == RowEntry::Notice) {
        switch (role) {
        case Qt::DisplayRole:
        case DisplayNameRole:
            return QStringLiteral("好友通知");
        case IsNoticeRole:
            return true;
        case IsGroupRole:
            return false;
        case Qt::SizeHintRole:
            return QSize(0, kGroupHeaderHeight);
        default:
            return {};
        }
    }

    if (row.groupIndex < 0 || row.groupIndex >= m_groups.size()) {
        return {};
    }

    const FriendGroup& group = m_groups.at(row.groupIndex);
    if (row.isGroup) {
        switch (role) {
        case Qt::DisplayRole:
        case DisplayNameRole:
        case GroupNameRole:
            return group.groupName;
        case IsGroupRole:
            return true;
        case IsNoticeRole:
            return false;
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

    const FriendSummary& friendSummary = group.friends.at(row.friendIndex);
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return friendSummary.displayName;
    case UserIdRole:
        return friendSummary.userId;
    case AvatarPathRole:
        return friendSummary.avatarPath;
    case StatusRole:
        return static_cast<int>(friendSummary.status);
    case SignatureRole:
        return friendSummary.signature;
    case DoNotDisturbRole:
        return friendSummary.isDoNotDisturb;
    case NickNameRole:
        return friendSummary.nickName.isEmpty() ? friendSummary.displayName : friendSummary.nickName;
    case RemarkRole:
        return friendSummary.remark;
    case IsGroupRole:
        return false;
    case IsNoticeRole:
        return false;
    case GroupIdRole:
        return group.groupId;
    case GroupNameRole:
        return group.groupName;
    case GroupProgressRole:
        return group.progress;
    case Qt::SizeHintRole:
        return QSize(0, qRound(kFriendItemHeight * group.progress));
    default:
        return {};
    }
}

Qt::ItemFlags FriendListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    if (isNoticeRow(index)) {
        return Qt::ItemIsEnabled;
    }

    if (isGroupRow(index)) {
        return Qt::ItemIsEnabled;
    }

    if (index.data(GroupProgressRole).toReal() <= 0.01) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void FriendListModel::setGroups(QVector<FriendGroupSummary> groups, bool preserveLoadedItems)
{
    QHash<QString, bool> expandedByGroup;
    QHash<QString, qreal> progressByGroup;
    QHash<QString, QVector<FriendSummary>> friendsByGroup;
    QHash<QString, bool> hasMoreByGroup;
    for (const FriendGroup& group : std::as_const(m_groups)) {
        expandedByGroup.insert(group.groupId, group.expanded);
        progressByGroup.insert(group.groupId, group.progress);
        friendsByGroup.insert(group.groupId, group.friends);
        hasMoreByGroup.insert(group.groupId, group.hasMore);
    }

    beginResetModel();
    m_groups.clear();
    for (const FriendGroupSummary& summary : std::as_const(groups)) {
        FriendGroup group;
        group.groupId = summary.groupId.isEmpty() ? QStringLiteral("default") : summary.groupId;
        group.groupName = summary.groupName.isEmpty() ? QStringLiteral("默认分组") : summary.groupName;
        group.totalCount = summary.friendCount;
        group.expanded = preserveLoadedItems && expandedByGroup.value(group.groupId, false);
        group.progress = progressByGroup.value(group.groupId, group.expanded ? 1.0 : 0.0);
        if (preserveLoadedItems) {
            group.friends = friendsByGroup.value(group.groupId);
        }
        if (group.friends.size() > group.totalCount) {
            group.friends.resize(group.totalCount);
        }
        group.hasMore = preserveLoadedItems
                ? hasMoreByGroup.value(group.groupId, group.friends.size() < group.totalCount)
                : group.totalCount > 0;
        if (!group.expanded && group.progress <= 0.01) {
            group.friends.clear();
            group.hasMore = group.totalCount > 0;
        }
        m_groups.push_back(group);
    }

    rebuildRows();
    endResetModel();
}

void FriendListModel::appendFriendsToGroup(const QString& groupId, QVector<FriendSummary> friends, bool hasMore)
{
    FriendGroup* group = groupForId(groupId);
    if (!group || friends.isEmpty() && group->hasMore == hasMore) {
        return;
    }

    beginResetModel();
    for (FriendSummary& friendSummary : friends) {
        friendSummary.groupId = group->groupId;
        friendSummary.groupName = group->groupName;
        bool duplicate = false;
        for (const FriendSummary& loaded : std::as_const(group->friends)) {
            if (loaded.userId == friendSummary.userId) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) {
            group->friends.push_back(friendSummary);
        }
    }
    group->hasMore = hasMore && group->friends.size() < group->totalCount;
    rebuildRows();
    endResetModel();
}

int FriendListModel::loadedFriendCount(const QString& groupId) const
{
    const FriendGroup* group = groupForId(groupId);
    return group ? group->friends.size() : 0;
}

bool FriendListModel::hasMoreFriends(const QString& groupId) const
{
    const FriendGroup* group = groupForId(groupId);
    return group ? group->hasMore : false;
}

void FriendListModel::pruneCollapsedRows()
{
    beginResetModel();
    rebuildRows();
    endResetModel();
}

QString FriendListModel::friendIdAt(const QModelIndex& index) const
{
    if (!isFriendRow(index)) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_groups.at(row.groupIndex).friends.at(row.friendIndex).userId;
}

QString FriendListModel::groupIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    if (row.groupIndex < 0 || row.groupIndex >= m_groups.size()) {
        return {};
    }
    return m_groups.at(row.groupIndex).groupId;
}

QString FriendListModel::groupIdForFriend(const QString& userId) const
{
    for (const FriendGroup& group : m_groups) {
        for (const FriendSummary& friendSummary : group.friends) {
            if (friendSummary.userId == userId) {
                return group.groupId;
            }
        }
    }
    return {};
}

FriendSummary FriendListModel::friendAt(const QModelIndex& index) const
{
    if (!isFriendRow(index)) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_groups.at(row.groupIndex).friends.at(row.friendIndex);
}

int FriendListModel::indexOfFriend(const QString& userId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const RowEntry& entry = m_rows.at(row);
        if (entry.isGroup || entry.groupIndex < 0 || entry.groupIndex >= m_groups.size()) {
            continue;
        }
        const FriendGroup& group = m_groups.at(entry.groupIndex);
        if (entry.friendIndex >= 0 && entry.friendIndex < group.friends.size() &&
            group.friends.at(entry.friendIndex).userId == userId) {
            return row;
        }
    }
    return -1;
}

int FriendListModel::groupRowForId(const QString& groupId) const
{
    return rowForGroup(groupId);
}

int FriendListModel::groupRowForRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    for (int current = row; current >= 0; --current) {
        if (m_rows.at(current).type == RowEntry::Group) {
            return current;
        }
    }
    return -1;
}

int FriendListModel::nextGroupRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    for (int current = row + 1; current < m_rows.size(); ++current) {
        if (m_rows.at(current).type == RowEntry::Group) {
            return current;
        }
    }
    return -1;
}

bool FriendListModel::isGroupRow(const QModelIndex& index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
           m_rows.at(index.row()).type == RowEntry::Group;
}

bool FriendListModel::isFriendRow(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return false;
    }

    const RowEntry& row = m_rows.at(index.row());
    return row.type == RowEntry::Friend && row.groupIndex >= 0 && row.groupIndex < m_groups.size() &&
           row.friendIndex >= 0 && row.friendIndex < m_groups.at(row.groupIndex).friends.size();
}

bool FriendListModel::isNoticeRow(const QModelIndex& index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
           m_rows.at(index.row()).type == RowEntry::Notice;
}

bool FriendListModel::isGroupExpanded(const QString& groupId) const
{
    const FriendGroup* group = groupForId(groupId);
    return group ? group->expanded : false;
}

qreal FriendListModel::groupProgress(const QString& groupId) const
{
    const FriendGroup* group = groupForId(groupId);
    return group ? group->progress : 0.0;
}

void FriendListModel::setGroupExpanded(const QString& groupId, bool expanded)
{
    FriendGroup* group = groupForId(groupId);
    if (!group || group->expanded == expanded) {
        return;
    }

    beginResetModel();
    group->expanded = expanded;
    rebuildRows();
    endResetModel();
}

void FriendListModel::setGroupProgress(const QString& groupId, qreal progress)
{
    FriendGroup* group = groupForId(groupId);
    if (!group) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(group->progress, boundedProgress)) {
        return;
    }

    group->progress = boundedProgress;
    const int firstRow = rowForGroup(groupId);
    const int lastRow = lastRowForGroup(groupId);
    if (firstRow >= 0 && lastRow >= firstRow) {
        emit dataChanged(index(firstRow, 0),
                         index(lastRow, 0),
                         {GroupProgressRole, Qt::SizeHintRole});
    }
}

const FriendListModel::FriendGroup* FriendListModel::groupForId(const QString& groupId) const
{
    for (const FriendGroup& group : m_groups) {
        if (group.groupId == groupId) {
            return &group;
        }
    }
    return nullptr;
}

FriendListModel::FriendGroup* FriendListModel::groupForId(const QString& groupId)
{
    for (FriendGroup& group : m_groups) {
        if (group.groupId == groupId) {
            return &group;
        }
    }
    return nullptr;
}

void FriendListModel::rebuildRows()
{
    m_rows.clear();
    m_rows.push_back(RowEntry{RowEntry::Notice, false, -1, -1});
    for (int groupIndex = 0; groupIndex < m_groups.size(); ++groupIndex) {
        m_rows.push_back(RowEntry{RowEntry::Group, true, groupIndex, -1});
        const FriendGroup& group = m_groups.at(groupIndex);
        if (!group.expanded && group.progress <= 0.01) {
            continue;
        }
        for (int friendIndex = 0; friendIndex < group.friends.size(); ++friendIndex) {
            m_rows.push_back(RowEntry{RowEntry::Friend, false, groupIndex, friendIndex});
        }
    }
}

int FriendListModel::rowForGroup(const QString& groupId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const RowEntry& entry = m_rows.at(row);
        if (entry.type == RowEntry::Group && entry.groupIndex >= 0 && entry.groupIndex < m_groups.size() &&
            m_groups.at(entry.groupIndex).groupId == groupId) {
            return row;
        }
    }
    return -1;
}

int FriendListModel::lastRowForGroup(const QString& groupId) const
{
    const int groupRow = rowForGroup(groupId);
    if (groupRow < 0) {
        return -1;
    }

    for (int row = groupRow + 1; row < m_rows.size(); ++row) {
        if (m_rows.at(row).type == RowEntry::Group) {
            return row - 1;
        }
    }
    return m_rows.size() - 1;
}
