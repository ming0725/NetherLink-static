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

    QVector<Group> requestGroupList(const GroupListRequest& query = {}) const;
    QVector<GroupCategorySummary> requestGroupCategorySummaries(const GroupCategoryListRequest& query = {}) const;
    QVector<Group> requestGroupsInCategory(const GroupCategoryItemsRequest& query) const;
    Group requestGroupDetail(const GroupDetailRequest& query) const;
    QString requestGroupAvatarPath(const QString& groupId) const;
    QMap<QString, QString> requestGroupCategories() const;
    QString effectiveGroupCategoryId(const Group& group) const;
    QString effectiveGroupCategoryName(const Group& group) const;
    bool isCurrentUserGroupOwner(const Group& group) const;
    bool isCurrentUserGroupAdmin(const Group& group) const;
    bool contains(const QString& groupId) const;

    void saveGroup(const Group& group);
    void removeGroup(const QString& groupID);

signals:
    void groupListChanged();

private:
    explicit GroupRepository(QObject* parent = nullptr);
    Q_DISABLE_COPY(GroupRepository)

    QMap<QString, Group> groupMap;
    mutable QMutex mutex; // 用于线程安全
};
