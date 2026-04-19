#include "UserRepository.h"
#include <QPainter>
#include <QPainterPath>

UserRepository::UserRepository(QObject* parent)
    : QObject(parent)
{
    QPixmapCache::setCacheLimit(20480);
    QVector<User> users = {
            {"u001", "momo",   "", ":/resources/avatar/1.jpg",   Online,  "我是好momo"},
            {"u002", "blazer",     "", ":/resources/avatar/0.jpg",     Mining, "不掉烈焰棒"},
            {"u003", "不吃香菜（考研版）", "", ":/resources/avatar/3.jpg", Online,  "绝不吃一口香菜！"},
            {"u004", "不抽香烟（一路菜花版）",   "", ":/resources/avatar/5.jpg",   Online,  "芝士雪豹"},
            {"u005", "不会演戏（许仙版）",   "", ":/resources/avatar/4.jpg",   Offline, "不碍事的白姑娘"},
            {"u006", "TralaleroTralala",   "", ":/resources/avatar/2.jpg",   Online,  "不穿Nike（蓝勾版）"},
            {"u007", "圆头耄耋",  "", ":/resources/avatar/6.jpg",  Offline,  "这只猫很懒，什么都没留下来"},
            {"u008", "歪比巴卜",   "", ":/resources/avatar/7.jpg",   Mining,  "歪比歪比"},
            {"u009", "tung tung tung sahur",     "", ":/resources/avatar/8.jpg",     Offline, "tung tung tung tung tung tung tung tung tung sahur"},
            {"u010", "BombardinoCrocodilo",   "", ":/resources/avatar/9.jpg",   Flying,  "已塌房"},
    };

    for (const User& user : users) {
        userMap.insert(user.id, user);
    }
}

UserRepository& UserRepository::instance() {
    static UserRepository repo;
    return repo;
}

User UserRepository::getUser(const QString& userID) {
    QMutexLocker locker(&mutex);
    return userMap.value(userID, User());
}

QVector<User> UserRepository::getAllUser() {
    QMutexLocker locker(&mutex);
    return QVector<User>::fromList(userMap.values());
}

void UserRepository::insertUser(const User& user) {
    QMutexLocker locker(&mutex);
    userMap[user.id] = user;
    cacheAvatar(user.id);
}

void UserRepository::removeUser(const QString& userID) {
    QMutexLocker locker(&mutex);
    userMap.remove(userID);
}

QString UserRepository::getName(QString userID) {
    return userMap.value(userID, User()).nick;
}

QPixmap UserRepository::getAvatar(const QString &userID) {
    const QString key = QString("avatar_%1").arg(userID);
    QPixmap pixmap;
    if (QPixmapCache::find(key, &pixmap)) {
        return pixmap;
    }
    // 未缓存则生成并缓存一次
    QMutexLocker locker(&mutex);
    if (userMap.contains(userID)) {
        cacheAvatar(userID);
        QPixmapCache::find(key, &pixmap);
    }
    return pixmap;
}

void UserRepository::cacheAvatar(const QString &userID) {
    const QString key = QString("avatar_%1").arg(userID);
    QPixmap original(userMap[userID].avatarPath);
    if (original.isNull()) return;

    QPixmap rounded(avatarSize, avatarSize);
    rounded.fill(Qt::transparent);

    QPainter painter(&rounded);
    painter.setRenderHint(QPainter::Antialiasing);
    QPainterPath path;
    path.addEllipse(0, 0, avatarSize, avatarSize);
    painter.setClipPath(path);
    painter.drawPixmap(0, 0, original.scaled(avatarSize, avatarSize,
                                             Qt::KeepAspectRatioByExpanding,
                                             Qt::SmoothTransformation));

    QPixmapCache::insert(key, rounded);
}

QString statusText(UserStatus userStatus) {
    if (userStatus == Online) return "在线";
    if (userStatus == Offline) return "离线";
    if (userStatus == Flying) return "飞行模式";
    else return "挖矿中";
}

QString statusIconPath(UserStatus userStatus) {
    QString prefix = ":/resources/icon/";
    if (userStatus == Online) return prefix + "online.png";
    if (userStatus == Offline) return prefix + "offline.png";
    if (userStatus == Flying) return prefix + "flying.png";
    else return prefix + "mining.png";
}