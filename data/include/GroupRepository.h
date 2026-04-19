#pragma once
#include <QObject>
#include <QMap>
#include <QVector>
#include <QMutex>
#include "Group.h"

class GroupRepository : public QObject{
    Q_OBJECT
public:
    static GroupRepository& instance();

    Group getGroup(const QString& groupID);
    QVector<Group> getAllGroup();

    // 可选添加：用户插入、删除接口
    void insertGroup(const Group& group);
    void removeGroup(const QString& groupID);
    bool isGroup(QString& id);

private:
    explicit GroupRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRepository)

    QMap<QString, Group> groupMap;
    QMutex mutex; // 用于线程安全
};

