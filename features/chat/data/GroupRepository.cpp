#include "GroupRepository.h"

#include <QCollator>
#include <QStringList>

#include <algorithm>

#include "app/state/CurrentUser.h"
#include "shared/data/RepositoryTemplate.h"

namespace {

const QString kCreatedCategoryId = QStringLiteral("gg_created");
const QString kCreatedCategoryName = QStringLiteral("我创建的群聊");
const QString kManagedCategoryId = QStringLiteral("gg_managed");
const QString kManagedCategoryName = QStringLiteral("我管理的群聊");
const QString kJoinedCategoryId = QStringLiteral("gg_joined");
const QString kJoinedCategoryName = QStringLiteral("我加入的群聊");
const QString kCollegeCategoryId = QStringLiteral("gg_college");
const QString kCollegeCategoryName = QStringLiteral("大学");
const QString kWorkCategoryId = QStringLiteral("gg_work");
const QString kWorkCategoryName = QStringLiteral("工作");
const QString kPerformanceCategoryId = QStringLiteral("gg_performance");
const QString kPerformanceCategoryName = QStringLiteral("性能测试");

QString currentUserId()
{
    return CurrentUser::instance().getUserId();
}

bool isCurrentUserOwnerOf(const Group& group)
{
    return !group.ownerId.isEmpty() && group.ownerId == currentUserId();
}

bool isCurrentUserAdminOf(const Group& group)
{
    const QString userId = currentUserId();
    return !userId.isEmpty() && group.adminsID.contains(userId);
}

QString normalizedBaseCategoryId(const Group& group)
{
    return group.listGroupId.isEmpty() ? kJoinedCategoryId : group.listGroupId;
}

QString normalizedBaseCategoryName(const Group& group)
{
    return group.listGroupName.isEmpty() ? kJoinedCategoryName : group.listGroupName;
}

void appendListEntry(QVector<Group>& groups,
                     const Group& group,
                     const QString& categoryId,
                     const QString& categoryName)
{
    Group normalized = group;
    normalized.listGroupId = categoryId;
    normalized.listGroupName = categoryName;
    groups.push_back(normalized);
}

QString effectiveCategoryIdFor(const Group& group)
{
    if (isCurrentUserOwnerOf(group)) {
        return kCreatedCategoryId;
    }
    if (isCurrentUserAdminOf(group)) {
        return kManagedCategoryId;
    }
    return normalizedBaseCategoryId(group);
}

QString effectiveCategoryNameFor(const Group& group)
{
    if (isCurrentUserOwnerOf(group)) {
        return kCreatedCategoryName;
    }
    if (isCurrentUserAdminOf(group)) {
        return kManagedCategoryName;
    }
    return normalizedBaseCategoryName(group);
}

int categorySortRank(const QString& categoryId)
{
    if (categoryId == kCreatedCategoryId) {
        return 0;
    }
    if (categoryId == kManagedCategoryId) {
        return 1;
    }
    if (categoryId == kJoinedCategoryId) {
        return 2;
    }
    if (categoryId == kCollegeCategoryId) {
        return 3;
    }
    if (categoryId == kWorkCategoryId) {
        return 4;
    }
    if (categoryId == kPerformanceCategoryId) {
        return 5;
    }
    return 99;
}

QString groupVisibleName(const Group& group)
{
    if (group.remark.isEmpty() || group.groupName.isEmpty()) {
        return group.groupName;
    }
    return QStringLiteral("%1(%2)").arg(group.remark, group.groupName);
}

QString groupVisibleSubtitle(const Group& group)
{
    return QStringLiteral("%1人").arg(group.memberNum);
}

void sortGroupList(QVector<Group>& result)
{
    std::sort(result.begin(), result.end(), [](const Group& lhs, const Group& rhs) {
        const int lhsCategoryRank = categorySortRank(lhs.listGroupId);
        const int rhsCategoryRank = categorySortRank(rhs.listGroupId);
        if (lhsCategoryRank != rhsCategoryRank) {
            return lhsCategoryRank < rhsCategoryRank;
        }
        if (lhs.listGroupName != rhs.listGroupName) {
            static QCollator groupCollator(QLocale::Chinese);
            groupCollator.setNumericMode(true);
            return groupCollator.compare(lhs.listGroupName, rhs.listGroupName) < 0;
        }

        static QCollator localCollator(QLocale::Chinese);
        localCollator.setNumericMode(true);
        const int nameOrder = localCollator.compare(groupVisibleName(lhs), groupVisibleName(rhs));
        if (nameOrder != 0) {
            return nameOrder < 0;
        }

        const int subtitleOrder = localCollator.compare(groupVisibleSubtitle(lhs), groupVisibleSubtitle(rhs));
        if (subtitleOrder != 0) {
            return subtitleOrder < 0;
        }
        return lhs.groupId < rhs.groupId;
    });
}

class GroupListRequestOperation final
    : public RepositoryTemplate<GroupListRequest, QVector<Group>> {
public:
    explicit GroupListRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<Group> doRequest(const GroupListRequest& query) const override
    {
        QVector<Group> result;
        result.reserve(m_groups.size() * 2);
        for (const Group& group : m_groups) {
            const QString baseCategoryId = normalizedBaseCategoryId(group);
            const QString baseCategoryName = normalizedBaseCategoryName(group);
            if (!query.keyword.isEmpty() &&
                !group.groupName.contains(query.keyword, Qt::CaseInsensitive) &&
                !group.remark.contains(query.keyword, Qt::CaseInsensitive) &&
                !effectiveCategoryNameFor(group).contains(query.keyword, Qt::CaseInsensitive) &&
                !baseCategoryName.contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }

            if (isCurrentUserOwnerOf(group)) {
                appendListEntry(result, group, kCreatedCategoryId, kCreatedCategoryName);
            } else if (isCurrentUserAdminOf(group)) {
                appendListEntry(result, group, kManagedCategoryId, kManagedCategoryName);
            }
            appendListEntry(result, group, baseCategoryId, baseCategoryName);
        }
        return result;
    }

    void onAfterRequest(const GroupListRequest&, QVector<Group>& result) const override
    {
        sortGroupList(result);
    }

    QVector<Group> m_groups;
};

class GroupCategoryListRequestOperation final
    : public RepositoryTemplate<GroupCategoryListRequest, QVector<GroupCategorySummary>> {
public:
    explicit GroupCategoryListRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<GroupCategorySummary> doRequest(const GroupCategoryListRequest& query) const override
    {
        const QVector<Group> groups = GroupListRequestOperation(m_groups).request({query.keyword});
        QMap<QString, GroupCategorySummary> categories;
        for (const Group& group : groups) {
            GroupCategorySummary summary = categories.value(group.listGroupId);
            summary.categoryId = group.listGroupId;
            summary.categoryName = group.listGroupName;
            ++summary.groupCount;
            categories.insert(group.listGroupId, summary);
        }
        return QVector<GroupCategorySummary>::fromList(categories.values());
    }

    void onAfterRequest(const GroupCategoryListRequest&, QVector<GroupCategorySummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const GroupCategorySummary& lhs, const GroupCategorySummary& rhs) {
            const int lhsRank = categorySortRank(lhs.categoryId);
            const int rhsRank = categorySortRank(rhs.categoryId);
            if (lhsRank != rhsRank) {
                return lhsRank < rhsRank;
            }

            static QCollator collator(QLocale::Chinese);
            collator.setNumericMode(true);
            const int order = collator.compare(lhs.categoryName, rhs.categoryName);
            return order == 0 ? lhs.categoryId < rhs.categoryId : order < 0;
        });
    }

    QVector<Group> m_groups;
};

class GroupCategoryItemsRequestOperation final
    : public RepositoryTemplate<GroupCategoryItemsRequest, QVector<Group>> {
public:
    explicit GroupCategoryItemsRequestOperation(QVector<Group> groups)
        : m_groups(std::move(groups))
    {
    }

private:
    QVector<Group> doRequest(const GroupCategoryItemsRequest& query) const override
    {
        QVector<Group> result;
        const QVector<Group> groups = GroupListRequestOperation(m_groups).request({query.keyword});
        for (const Group& group : groups) {
            if (group.listGroupId == query.categoryId) {
                result.push_back(group);
            }
        }
        return result;
    }

    void onAfterRequest(const GroupCategoryItemsRequest& query, QVector<Group>& result) const override
    {
        const int offset = qBound(0, query.offset, result.size());
        const int limit = query.limit < 0 ? result.size() - offset : qMax(0, query.limit);
        result = result.mid(offset, limit);
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

Group makeGroup(const QString& groupId,
                const QString& groupName,
                int memberNum,
                const QString& ownerId,
                const QString& avatarPath,
                const QString& listGroupId,
                const QString& listGroupName,
                const QString& remark = QString(),
                bool isDnd = false,
                QVector<QString> adminsID = {})
{
    Group group;
    group.groupId = groupId;
    group.groupName = groupName;
    group.memberNum = memberNum;
    group.ownerId = ownerId;
    group.groupAvatarPath = avatarPath;
    group.isDnd = isDnd;
    group.adminsID = std::move(adminsID);
    group.remark = remark;
    group.introduction = QStringLiteral("%1，用于日常交流、资料同步和协作安排。").arg(groupName);
    group.announcement = QStringLiteral("欢迎来到%1，请保持友好沟通。").arg(groupName);
    group.listGroupId = listGroupId;
    group.listGroupName = listGroupName;
    return group;
}

} // namespace

GroupRepository::GroupRepository(QObject* parent)
        : QObject(parent)
{
    const QVector<Group> groups = {
            makeGroup("g001", "相亲相爱一家人", 55, "u007", ":/resources/avatar/10.jpg",
                      kJoinedCategoryId, kJoinedCategoryName, "家庭群"),
            makeGroup("g002", "考研交流群", 11, "u001", ":/resources/avatar/10.jpg",
                      kCollegeCategoryId, kCollegeCategoryName),
            makeGroup("g003", "暗广接单群", 91, "u002", ":/resources/avatar/10.jpg",
                      kWorkCategoryId, kWorkCategoryName, "接单群"),
            makeGroup("g004", "Qt学习交流群", 103, "u002", ":/resources/avatar/10.jpg",
                      kCollegeCategoryId, kCollegeCategoryName, "Qt 群", false, {"u007"}),
            makeGroup("g005", "C/C++技术交流", 78, "u004", ":/resources/avatar/10.jpg",
                      kWorkCategoryId, kWorkCategoryName, QString(), false, {"u007"}),
            makeGroup("g006", "毕业设计小组", 5, "u003", ":/resources/avatar/10.jpg",
                      kCollegeCategoryId, kCollegeCategoryName, "毕设组"),
            makeGroup("g007", "前端/UI交流", 46, "u005", ":/resources/avatar/10.jpg",
                      kWorkCategoryId, kWorkCategoryName),
            makeGroup("g008", "算法竞赛交流", 58, "u006", ":/resources/avatar/10.jpg",
                      kCollegeCategoryId, kCollegeCategoryName, "算法群"),
            makeGroup("g009", "A346宿舍", 4, "u007", ":/resources/avatar/10.jpg",
                      kJoinedCategoryId, kJoinedCategoryName, "宿舍群"),
            makeGroup("g010", "服务器维护群", 18, "u007", ":/resources/avatar/10.jpg",
                      kWorkCategoryId, kWorkCategoryName),
            makeGroup("g011", "下界探险队", 24, "u008", ":/resources/avatar/10.jpg",
                      kJoinedCategoryId, kJoinedCategoryName, "探险队"),
            makeGroup("g012", "像素美术互助", 39, "u005", ":/resources/avatar/10.jpg",
                      kWorkCategoryId, kWorkCategoryName, QString(), false, {"u007"}),
            makeGroup("g013", "红石机器研究所", 67, "u010", ":/resources/avatar/10.jpg",
                      kJoinedCategoryId, kJoinedCategoryName, "红石群"),
            makeGroup("g014", "周末开黑车队", 12, "u007", ":/resources/avatar/10.jpg",
                      kJoinedCategoryId, kJoinedCategoryName),
    };

    for (const Group& group : groups) {
        groupMap.insert(group.groupId, group);
    }

    auto addPerformanceGroup = [this](const QString& listGroupId,
                                      const QString& listGroupName,
                                      int count,
                                      int idOffset) {
        const QStringList names = {
                "性能测试群",
                "压力测试频道",
                "多人协作大厅",
                "渲染性能观测",
                "长列表滚动群",
                "虚拟化采样群"
        };
        const QStringList owners = {
                "u001",
                "u002",
                "u003",
                "u004"
        };

        for (int index = 0; index < count; ++index) {
            const int serial = idOffset + index;
            Group group = makeGroup(QString("gperf_%1").arg(serial, 4, 10, QChar('0')),
                                    QString("%1 %2").arg(names.at(index % names.size())).arg(index + 1),
                                    20 + (index * 13) % 480,
                                    owners.at(index % owners.size()),
                                    QString(":/resources/avatar/%1.jpg").arg((index + 1) % 11),
                                    listGroupId,
                                    listGroupName,
                                    (index % 7 == 0)
                                            ? QString("备注群 %1").arg(index + 1)
                                            : QString());
            groupMap.insert(group.groupId, group);
        }
    };

    addPerformanceGroup(kPerformanceCategoryId, kPerformanceCategoryName, 24, 1000);
    addPerformanceGroup(kPerformanceCategoryId, kPerformanceCategoryName, 128, 2000);
    addPerformanceGroup(kPerformanceCategoryId, kPerformanceCategoryName, 560, 3000);
}

GroupRepository& GroupRepository::instance()
{
    static GroupRepository repo;
    return repo;
}

QVector<Group> GroupRepository::requestGroupList(const GroupListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupListRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
}

QVector<GroupCategorySummary> GroupRepository::requestGroupCategorySummaries(const GroupCategoryListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupCategoryListRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
}

QVector<Group> GroupRepository::requestGroupsInCategory(const GroupCategoryItemsRequest& query) const
{
    QMutexLocker locker(&mutex);
    return GroupCategoryItemsRequestOperation(QVector<Group>::fromList(groupMap.values())).request(query);
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

QMap<QString, QString> GroupRepository::requestGroupCategories() const
{
    return {
            {kJoinedCategoryId, kJoinedCategoryName},
            {kCollegeCategoryId, kCollegeCategoryName},
            {kWorkCategoryId, kWorkCategoryName},
            {kPerformanceCategoryId, kPerformanceCategoryName}
    };
}

QString GroupRepository::effectiveGroupCategoryId(const Group& group) const
{
    return effectiveCategoryIdFor(group);
}

QString GroupRepository::effectiveGroupCategoryName(const Group& group) const
{
    return effectiveCategoryNameFor(group);
}

bool GroupRepository::isCurrentUserGroupOwner(const Group& group) const
{
    return isCurrentUserOwnerOf(group);
}

bool GroupRepository::isCurrentUserGroupAdmin(const Group& group) const
{
    return isCurrentUserAdminOf(group);
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
    locker.unlock();
    emit groupListChanged();
}

void GroupRepository::removeGroup(const QString& groupID)
{
    QMutexLocker locker(&mutex);
    const bool removed = groupMap.remove(groupID) > 0;
    locker.unlock();
    if (removed) {
        emit groupListChanged();
    }
}
