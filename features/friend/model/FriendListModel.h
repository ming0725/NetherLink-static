#pragma once

#include <QAbstractListModel>
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
        DoNotDisturbRole
    };

    explicit FriendListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setFriends(QVector<FriendSummary> friends);
    QString friendIdAt(const QModelIndex& index) const;
    FriendSummary friendAt(const QModelIndex& index) const;
    int indexOfFriend(const QString& userId) const;

private:
    QVector<FriendSummary> m_friends;
};
