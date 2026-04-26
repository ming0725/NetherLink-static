#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/User.h"

class UserRepository : public QObject {
    Q_OBJECT
public:
    static UserRepository& instance();

    QVector<FriendSummary> requestFriendList(const FriendListRequest& query = {}) const;
    QVector<FriendGroupSummary> requestFriendGroupSummaries(const FriendGroupListRequest& query = {}) const;
    QVector<FriendSummary> requestFriendsInGroup(const FriendGroupItemsRequest& query) const;
    User requestUserDetail(const UserDetailRequest& query) const;
    QMap<QString, QString> requestFriendGroups() const;
    QString requestUserName(const QString& userId) const;
    QString requestUserAvatarPath(const QString& userId) const;

    void saveUser(const User& user);
    void removeUser(const QString& userID);

signals:
    void friendListChanged();

private:
    explicit UserRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(UserRepository)
    QMap<QString, User> userMap;
    mutable QMutex mutex; // 用于线程安全
};
