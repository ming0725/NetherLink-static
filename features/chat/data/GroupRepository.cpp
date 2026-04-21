#include "GroupRepository.h"

#include "shared/data/RepositoryTemplate.h"

namespace {

class GroupListRequestOperation final
    : public RepositoryTemplate<EmptyRequest, QVector<Group>> {
public:
    explicit GroupListRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<Group> doRequest(const EmptyRequest&) const override
    {
        return m_groups;
    }

    QVector<Group> m_groups;
};

class GroupDetailRequestOperation final
    : public RepositoryTemplate<GroupDetailRequest, Group> {
public:
    explicit GroupDetailRequestOperation(QMap<QString, Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    Group doRequest(const GroupDetailRequest& query) const override
    {
        return m_groups.value(query.groupId, Group{});
    }

    QMap<QString, Group> m_groups;
};

} // namespace

GroupRepository::GroupRepository(QObject* parent)
        : QObject(parent)
{
    const QVector<Group> groups = {
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
}

GroupRepository& GroupRepository::instance()
{
    static GroupRepository repo;
    return repo;
}

QVector<Group> GroupRepository::requestGroupList() const
{
    QMutexLocker locker(&mutex);
    return GroupListRequestOperation(QVector<Group>::fromList(groupMap.values())).request({});
}

Group GroupRepository::requestGroupDetail(const GroupDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupDetailRequestOperation(groupMap).request(query);
}

QString GroupRepository::requestGroupAvatarPath(const QString& groupId) const
{
    return requestGroupDetail({groupId}).groupAvatarPath;
}

bool GroupRepository::contains(const QString& groupId) const
{
    QMutexLocker locker(&mutex);
    return groupMap.contains(groupId);
}

void GroupRepository::saveGroup(const Group& group)
{
    QMutexLocker locker(&mutex);
    groupMap[group.groupId] = group;
}

void GroupRepository::removeGroup(const QString& groupID)
{
    QMutexLocker locker(&mutex);
    groupMap.remove(groupID);
}
