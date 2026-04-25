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
        case GroupIdRole:
            return group.groupId;
        case GroupFriendCountRole:
            return group.friends.size();
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
    case IsGroupRole:
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

    if (isGroupRow(index)) {
        return Qt::ItemIsEnabled;
    }

    if (index.data(GroupProgressRole).toReal() <= 0.01) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void FriendListModel::setFriends(QVector<FriendSummary> friends)
{
    QHash<QString, bool> expandedByGroup;
    QHash<QString, qreal> progressByGroup;
    for (const FriendGroup& group : std::as_const(m_groups)) {
        expandedByGroup.insert(group.groupId, group.expanded);
        progressByGroup.insert(group.groupId, group.progress);
    }

    beginResetModel();
    m_groups.clear();
    QHash<QString, int> groupRows;
    for (const FriendSummary& friendSummary : std::as_const(friends)) {
        const QString groupId = normalizedGroupId(friendSummary);
        const QString groupName = normalizedGroupName(friendSummary);
        int groupIndex = groupRows.value(groupId, -1);
        if (groupIndex < 0) {
            FriendGroup group;
            group.groupId = groupId;
            group.groupName = groupName;
            group.expanded = expandedByGroup.value(groupId, false);
            group.progress = progressByGroup.value(groupId, group.expanded ? 1.0 : 0.0);
            m_groups.push_back(group);
            groupIndex = m_groups.size() - 1;
            groupRows.insert(groupId, groupIndex);
        }

        FriendSummary normalized = friendSummary;
        normalized.groupId = groupId;
        normalized.groupName = groupName;
        m_groups[groupIndex].friends.push_back(normalized);
    }

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
        if (m_rows.at(current).isGroup) {
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
        if (m_rows.at(current).isGroup) {
            return current;
        }
    }
    return -1;
}

bool FriendListModel::isGroupRow(const QModelIndex& index) const
{
    return index.isValid() && index.row() >= 0 && index.row() < m_rows.size() &&
           m_rows.at(index.row()).isGroup;
}

bool FriendListModel::isFriendRow(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return false;
    }

    const RowEntry& row = m_rows.at(index.row());
    return !row.isGroup && row.groupIndex >= 0 && row.groupIndex < m_groups.size() &&
           row.friendIndex >= 0 && row.friendIndex < m_groups.at(row.groupIndex).friends.size();
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

    group->expanded = expanded;
    const int groupRow = rowForGroup(groupId);
    if (groupRow >= 0) {
        const QModelIndex changed = index(groupRow, 0);
        emit dataChanged(changed, changed, {GroupExpandedRole});
    }
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
    for (int groupIndex = 0; groupIndex < m_groups.size(); ++groupIndex) {
        m_rows.push_back(RowEntry{true, groupIndex, -1});
        const FriendGroup& group = m_groups.at(groupIndex);
        for (int friendIndex = 0; friendIndex < group.friends.size(); ++friendIndex) {
            m_rows.push_back(RowEntry{false, groupIndex, friendIndex});
        }
    }
}

int FriendListModel::rowForGroup(const QString& groupId) const
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

int FriendListModel::lastRowForGroup(const QString& groupId) const
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
