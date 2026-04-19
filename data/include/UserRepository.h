#pragma once

#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include <QPixmapCache>
#include "User.h"

class UserRepository : public QObject {
    Q_OBJECT
public:
    static UserRepository& instance();
    User getUser(const QString& userID);
    QVector<User> getAllUser();
    void insertUser(const User& user);
    void removeUser(const QString& userID);
    QString getName(QString userID);
    QPixmap getAvatar(const QString& userID);

private:
    explicit UserRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(UserRepository)
    void cacheAvatar(const QString& userID);
    QMap<QString, User> userMap;
    QMutex mutex; // 用于线程安全
    const int avatarSize = 48;
};
