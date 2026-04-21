#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include "shared/types/RepositoryTypes.h"
#include "shared/types/Group.h"

class GroupRepository : public QObject{
    Q_OBJECT
public:
    static GroupRepository& instance();

    QVector<Group> requestGroupList() const;
    Group requestGroupDetail(const GroupDetailRequest& query) const;
    QString requestGroupAvatarPath(const QString& groupId) const;
    bool contains(const QString& groupId) const;

    void saveGroup(const Group& group);
    void removeGroup(const QString& groupID);

private:
    explicit GroupRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRepository)

    QMap<QString, Group> groupMap;
    mutable QMutex mutex; // 用于线程安全
};
