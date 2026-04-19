#include "GroupRepository.h"
#include <QPixmapCache>
#include <QPainterPath>
#include <QPainter>

GroupRepository::GroupRepository(QObject* parent)
        : QObject(parent)
{
    QVector<Group> groups = {
        {"g001", "相亲相爱一家人", 55, "u001", ":/resources/avatar/10.jpg"},
        {"g002", "考研交流群", 11, "u001", ":/resources/avatar/10.jpg"},
        {"g003", "暗广接单群", 91, "u002", ":/resources/avatar/10.jpg"},
        {"g004", "QT学习交流群", 103, "u002", ":/resources/avatar/10.jpg"},
        {"g005", "C/C++技术交流", 78, "u004", ":/resources/avatar/10.jpg"},
        {"g006", "毕业设计小组", 5, "u003", ":/resources/avatar/10.jpg"},
        {"g007", "前端/UI交流", 46, "u005", ":/resources/avatar/10.jpg"},
        {"g008", "算法竞赛交流", 58, "u006", ":/resources/avatar/10.jpg"},
        {"g009", "A346宿舍", 4, "u007", ":/resources/avatar/10.jpg"},
    };

    for (const Group& group : groups) {
        groupMap.insert(group.groupId, group);
    }
    const QString key = QString("group_avatar");
    const int avatarSize = 48;
    QPixmap original(groups[0].groupAvatarPath);
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

GroupRepository& GroupRepository::instance() {
    static GroupRepository repo;
    return repo;
}

Group GroupRepository::getGroup(const QString& groupID) {
    QMutexLocker locker(&mutex);
    return groupMap.value(groupID, Group());
}

QVector<Group> GroupRepository::getAllGroup() {
    QMutexLocker locker(&mutex);
    return QVector<Group>::fromList(groupMap.values());
}

void GroupRepository::insertGroup(const Group& group) {
    QMutexLocker locker(&mutex);
    groupMap[group.groupId] = group;
}

void GroupRepository::removeGroup(const QString& groupID) {
    QMutexLocker locker(&mutex);
    groupMap.remove(groupID);
}

bool GroupRepository::isGroup(QString &id) {
    QMutexLocker locker(&mutex);
    return groupMap.count(id) != 0;
}
