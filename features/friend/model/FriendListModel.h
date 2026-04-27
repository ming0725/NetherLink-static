#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QStringList>
#include <QVector>

#include "shared/types/RepositoryTypes.h"

class FriendListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        UserIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AvatarPathRole,
        StatusRole,
        SignatureRole,
        DoNotDisturbRole,
        NickNameRole,
        RemarkRole,
        IsNoticeRole,
        IsGroupRole,
        GroupIdRole,
        GroupNameRole,
        GroupFriendCountRole,
        GroupExpandedRole,
        GroupProgressRole,
        ContextMenuActiveRole
    };

    explicit FriendListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setGroups(QVector<FriendGroupSummary> groups, bool preserveLoadedItems = true);
    void appendFriendsToGroup(const QString& groupId, QVector<FriendSummary> friends, bool hasMore);
    void replaceFriendsInGroup(const QString& groupId, QVector<FriendSummary> friends, bool hasMore);
    QStringList groupIds() const;
    int loadedFriendCount(const QString& groupId) const;
    bool hasMoreFriends(const QString& groupId) const;
    void pruneCollapsedRows();
    QString friendIdAt(const QModelIndex& index) const;
    QString groupIdAt(const QModelIndex& index) const;
    QString groupIdForFriend(const QString& userId) const;
    FriendSummary friendAt(const QModelIndex& index) const;
    int indexOfFriend(const QString& userId) const;
    int groupRowForId(const QString& groupId) const;
    int groupRowForRow(int row) const;
    int nextGroupRow(int row) const;
    bool isGroupRow(const QModelIndex& index) const;
    bool isFriendRow(const QModelIndex& index) const;
    bool isNoticeRow(const QModelIndex& index) const;
    bool isGroupExpanded(const QString& groupId) const;
    qreal groupProgress(const QString& groupId) const;
    void setGroupExpanded(const QString& groupId, bool expanded);
    void setGroupProgress(const QString& groupId, qreal progress);
    void setContextMenuFriend(const QString& userId);

private:
    struct FriendGroup {
        QString groupId;
        QString groupName;
        QVector<FriendSummary> friends;
        int totalCount = 0;
        bool hasMore = false;
        bool expanded = false;
        qreal progress = 0.0;
    };

    struct RowEntry {
        enum Type {
            Notice,
            Group,
            Friend
        };

        Type type = Group;
        bool isGroup = false;
        int groupIndex = -1;
        int friendIndex = -1;
    };

    const FriendGroup* groupForId(const QString& groupId) const;
    FriendGroup* groupForId(const QString& groupId);
    void rebuildRows();
    int rowForGroup(const QString& groupId) const;
    int lastRowForGroup(const QString& groupId) const;

    QVector<FriendGroup> m_groups;
    QVector<RowEntry> m_rows;
    QString m_contextMenuFriendId;
};
