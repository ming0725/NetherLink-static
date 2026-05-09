#include "UserRepository.h"

#include <QCollator>
#include <QStringList>

#include <algorithm>

#include "shared/services/ImageService.h"
#include "shared/data/RepositoryTemplate.h"

namespace {

int statusSortRank(UserStatus status)
{
    switch (status) {
    case Online:
        return 0;
    case Mining:
        return 1;
    case Flying:
        return 2;
    case Offline:
    default:
        return 3;
    }
}

QString friendVisibleName(const FriendSummary& friendSummary)
{
    if (friendSummary.remark.isEmpty() || friendSummary.nickName.isEmpty()) {
        return friendSummary.displayName;
    }
    return QStringLiteral("%1(%2)").arg(friendSummary.remark, friendSummary.nickName);
}

QString friendVisibleSubtitle(const FriendSummary& friendSummary)
{
    return QStringLiteral("[%1] %2").arg(statusText(friendSummary.status), friendSummary.signature);
}

QString normalizedFriendGroupId(const User& user)
{
    return user.friendGroupId.isEmpty() ? QStringLiteral("default") : user.friendGroupId;
}

QString normalizedFriendGroupName(const User& user)
{
    return user.friendGroupName.isEmpty() ? QStringLiteral("默认分组") : user.friendGroupName;
}

bool matchesFriendKeyword(const User& user, const QString& keyword)
{
    if (!user.isFriend) {
        return false;
    }

    return keyword.isEmpty() ||
           user.nick.contains(keyword, Qt::CaseInsensitive) ||
           user.remark.contains(keyword, Qt::CaseInsensitive) ||
           user.signature.contains(keyword, Qt::CaseInsensitive) ||
           normalizedFriendGroupName(user).contains(keyword, Qt::CaseInsensitive);
}

FriendSummary makeFriendSummary(const User& user)
{
    const QString displayName = user.remark.isEmpty() ? user.nick : user.remark;
    return FriendSummary{
            user.id,
            displayName,
            user.avatarPath,
            user.status,
            user.signature,
            user.isDnd,
            normalizedFriendGroupId(user),
            normalizedFriendGroupName(user),
            user.nick,
            user.remark
    };
}

void sortFriendSummaries(QVector<FriendSummary>& result)
{
    std::sort(result.begin(), result.end(), [](const FriendSummary& lhs, const FriendSummary& rhs) {
        if (lhs.groupName != rhs.groupName) {
            static QCollator groupCollator(QLocale::Chinese);
            groupCollator.setNumericMode(true);
            return groupCollator.compare(lhs.groupName, rhs.groupName) < 0;
        }

        const int lhsStatusRank = statusSortRank(lhs.status);
        const int rhsStatusRank = statusSortRank(rhs.status);
        if (lhsStatusRank != rhsStatusRank) {
            return lhsStatusRank < rhsStatusRank;
        }

        static QCollator localCollator(QLocale::Chinese);
        localCollator.setNumericMode(true);
        const int nameOrder = localCollator.compare(friendVisibleName(lhs), friendVisibleName(rhs));
        if (nameOrder != 0) {
            return nameOrder < 0;
        }

        const int subtitleOrder = localCollator.compare(friendVisibleSubtitle(lhs), friendVisibleSubtitle(rhs));
        if (subtitleOrder != 0) {
            return subtitleOrder < 0;
        }
        return lhs.userId < rhs.userId;
    });
}

class FriendListRequestOperation final
    : public RepositoryTemplate<FriendListRequest, QVector<FriendSummary>> {
public:
    explicit FriendListRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendSummary> doRequest(const FriendListRequest& query) const override
    {
        QVector<FriendSummary> result;
        result.reserve(m_users.size());

        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            result.push_back(makeFriendSummary(user));
        }
        return result;
    }

    void onAfterRequest(const FriendListRequest&, QVector<FriendSummary>& result) const override
    {
        sortFriendSummaries(result);
    }

    QVector<User> m_users;
};

class FriendGroupListRequestOperation final
    : public RepositoryTemplate<FriendGroupListRequest, QVector<FriendGroupSummary>> {
public:
    explicit FriendGroupListRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendGroupSummary> doRequest(const FriendGroupListRequest& query) const override
    {
        QMap<QString, FriendGroupSummary> groups;
        for (const User& user : m_users) {
            if (!user.isFriend || !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }

            const QString groupId = normalizedFriendGroupId(user);
            FriendGroupSummary summary = groups.value(groupId);
            summary.groupId = groupId;
            summary.groupName = normalizedFriendGroupName(user);
            ++summary.friendCount;
            groups.insert(groupId, summary);
        }
        return QVector<FriendGroupSummary>::fromList(groups.values());
    }

    void onAfterRequest(const FriendGroupListRequest&, QVector<FriendGroupSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const FriendGroupSummary& lhs, const FriendGroupSummary& rhs) {
            static QCollator collator(QLocale::Chinese);
            collator.setNumericMode(true);
            const int order = collator.compare(lhs.groupName, rhs.groupName);
            return order == 0 ? lhs.groupId < rhs.groupId : order < 0;
        });
    }

    QVector<User> m_users;
};

class FriendGroupItemsRequestOperation final
    : public RepositoryTemplate<FriendGroupItemsRequest, QVector<FriendSummary>> {
public:
    explicit FriendGroupItemsRequestOperation(QVector<User> users)
        : m_users(std::move(users))
    {
    }

private:
    QVector<FriendSummary> doRequest(const FriendGroupItemsRequest& query) const override
    {
        QVector<FriendSummary> result;
        for (const User& user : m_users) {
            if (!user.isFriend ||
                normalizedFriendGroupId(user) != query.groupId ||
                !matchesFriendKeyword(user, query.keyword)) {
                continue;
            }
            result.push_back(makeFriendSummary(user));
        }
        return result;
    }

    void onAfterRequest(const FriendGroupItemsRequest& query, QVector<FriendSummary>& result) const override
    {
        sortFriendSummaries(result);
        const int offset = qBound(0, query.offset, result.size());
        const int limit = query.limit < 0 ? result.size() - offset : qMax(0, query.limit);
        result = result.mid(offset, limit);
    }

    QVector<User> m_users;
};

class UserDetailRequestOperation final
    : public RepositoryTemplate<UserDetailRequest, User> {
public:
    explicit UserDetailRequestOperation(QMap<QString, User> users)
        : m_users(std::move(users))
    {
    }

private:
    User doRequest(const UserDetailRequest& query) const override
    {
        return m_users.value(query.userId, User{});
    }

    QMap<QString, User> m_users;
};

} // namespace

UserRepository::UserRepository(QObject* parent)
    : QObject(parent)
{
    // Current user is owned by CurrentUserProfileRepository, not seeded as a friend/user row here.
    const QVector<User> users = {
            {"u001", "方块诗人", "桃木匠", ":/resources/avatar/1.jpg", Online, "今天的灵感从一块草方块开始", false, true, "g001", "建筑搭子", "上海"},
            {"u002", "烈焰建筑师", "", ":/resources/avatar/0.jpg", Mining, "用下界砖搭一座会发光的门厅", false, true, "g001", "建筑搭子", ""},
            {"u003", "红石逻辑师", "中继器同学", ":/resources/avatar/3.jpg", Online, "每条线路都要留一个观察者", false, true, "g001", "建筑搭子", "北京"},
            {"u004", "云杉小屋设计师", "", ":/resources/avatar/5.jpg", Online, "屋檐再低一格就有冒险感", false, true, "g002", "红石同伴", "杭州"},
            {"u005", "玻璃穹顶匠", "穹顶许愿池", ":/resources/avatar/4.jpg", Offline, "把星光封进彩色玻璃", false, true, "g002", "红石同伴", ""},
            {"u006", "海晶灯调色员", "", ":/resources/avatar/2.jpg", Online, "给主城调一盏柔和的灯", false, true, "g002", "红石同伴", "广州"},
            {"u008", "指令方块导演", "指令哥", ":/resources/avatar/7.jpg", Mining, "用一行命令点亮整座剧场", false, true, "g003", "服务器伙伴", "深圳"},
            {"u009", "蘑菇岛园艺师", "", ":/resources/avatar/8.jpg", Offline, "菌丝地也能开出花园", false, true, "g003", "服务器伙伴", ""},
            {"u010", "鞘翅巡景员", "", ":/resources/avatar/9.jpg", Flying, "从高空检查每一条屋脊", false, true, "g003", "服务器伙伴", "成都"},
            {"u011", "樱花庭院师", "临时设计稿", ":/resources/avatar/10.jpg", Online, "今天把庭院水渠改成心形", false, true, "g004", "临时设计稿", ""},
            {"u012", "刷怪塔调试员", "", ":/resources/avatar/0.jpg", Offline, "让怪物掉落，也让结构好看", false, true, "g004", "临时设计稿", "武汉"},
            {"u013", "下界砖雕刻师", "", ":/resources/avatar/3.jpg", Mining, "岩浆旁边最适合找配色", false, true, "g004", "临时设计稿", ""},
            {"u014", "信标灯塔守望者", "", ":/resources/avatar/7.jpg", Online, "收到请发一束信标光", false, true, "g005", "最近来访", "南京"},
            {"u015", "地图墙收藏家", "新建造好友", ":/resources/avatar/6.jpg", Flying, "刚刚交换了一张主城地图", false, true, "g005", "最近来访", ""},
            {"u016", "末地船修复师", "", ":/resources/avatar/2.jpg", Offline, "晚点在末地船坞碰头", false, true, "g005", "最近来访", "重庆"},
            {"u101", "创意服向导", "", ":/resources/avatar/10.jpg", Online, "验证来自创意建筑服", false, false, "default", "默认分组", "上海"},
            {"u102", "红石课堂同学", "", ":/resources/avatar/3.jpg", Mining, "一起调过隐藏门", false, false, "default", "默认分组", "北京"},
            {"u103", "方块花园旅人", "", ":/resources/avatar/5.jpg", Online, "朋友推荐我来参观你的工坊", false, false, "default", "默认分组", "杭州"},
            {"u104", "坐标寻路员", "", ":/resources/avatar/2.jpg", Offline, "通过建筑坐标搜索找到你", false, false, "default", "默认分组", ""},
            {"u105", "设计稿交换员", "", ":/resources/avatar/8.jpg", Flying, "来自作品集名片", false, false, "default", "默认分组", "深圳"},
    };

    for (const User& user : users) {
        userMap.insert(user.id, user);
    }

    const QStringList requestNames = {
            QStringLiteral("空岛规划师"),
            QStringLiteral("苔石园丁"),
            QStringLiteral("红石剧场助手"),
            QStringLiteral("像素雕像匠"),
            QStringLiteral("地形笔刷练习生"),
            QStringLiteral("主城道路测绘员"),
            QStringLiteral("村民交易所设计师"),
            QStringLiteral("海底基地灯光师"),
            QStringLiteral("末地花园访客"),
            QStringLiteral("命令方块实习生"),
            QStringLiteral("下界大厅铺砖工"),
            QStringLiteral("樱花塔楼摄影师")
    };
    const QVector<UserStatus> requestStatuses = {Online, Mining, Offline, Flying};
    for (int index = 5; index < 36; ++index) {
        const int serial = 101 + index;
        User user;
        user.id = QStringLiteral("u%1").arg(serial);
        user.nick = QStringLiteral("%1 %2").arg(requestNames.at(index % requestNames.size())).arg(index + 1);
        user.avatarPath = QStringLiteral(":/resources/avatar/%1.jpg").arg(index % 11);
        user.status = requestStatuses.at(index % requestStatuses.size());
        user.signature = QStringLiteral("正在准备击败 %1 次凋零").arg(index + 1);
        user.isFriend = false;
        userMap.insert(user.id, user);
    }

    auto addPerformanceGroup = [this](const QString& groupId,
                                      const QString& groupName,
                                      int count,
                                      int idOffset) {
        const QStringList names = {
                "石砖街区设计师",
                "红石电梯测试员",
                "橡木屋顶匠",
                "海底玻璃师",
                "末地桥梁师",
                "苔藓洞穴园丁",
                "信标广场策展人",
                "沙漠神殿修缮员"
        };
        const QVector<UserStatus> statuses = {
                Online,
                Mining,
                Offline,
                Flying
        };

        for (int index = 0; index < count; ++index) {
            const int serial = idOffset + index;
            User user{
                    QString("perf_%1").arg(serial, 4, 10, QChar('0')),
                    QString("%1 %2").arg(names.at(index % names.size())).arg(index + 1),
                    (index % 9 == 0) ? QString("红石搭子 %1").arg(index + 1) : QString(),
                    QString(":/resources/avatar/%1.jpg").arg(index % 11),
                    statuses.at(index % statuses.size()),
                    QString("创意服长列表样本 %1").arg(index + 1),
                    false,
                    true,
                    groupId,
                    groupName,
                    index % 3 == 0 ? QStringLiteral("杭州") :
                    (index % 3 == 1 ? QString() : QStringLiteral("深圳"))
            };
            userMap.insert(user.id, user);
        }
    };

    addPerformanceGroup("g101", "性能测试 20+", 24, 1000);
    addPerformanceGroup("g102", "性能测试 100+", 128, 2000);
    addPerformanceGroup("g103", "性能测试 500+", 560, 3000);
}

UserRepository& UserRepository::instance()
{
    static UserRepository repo;
    return repo;
}

QVector<FriendSummary> UserRepository::requestFriendList(const FriendListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return FriendListRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
}

QVector<FriendGroupSummary> UserRepository::requestFriendGroupSummaries(const FriendGroupListRequest& query) const
{
    QMutexLocker locker(&mutex);
    return FriendGroupListRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
}

QVector<FriendSummary> UserRepository::requestFriendsInGroup(const FriendGroupItemsRequest& query) const
{
    QMutexLocker locker(&mutex);
    return FriendGroupItemsRequestOperation(QVector<User>::fromList(userMap.values())).request(query);
}

User UserRepository::requestUserDetail(const UserDetailRequest& query) const
{
    QMutexLocker locker(&mutex);
    return UserDetailRequestOperation(userMap).request(query);
}

QString UserRepository::requestUserName(const QString& userId) const
{
    return requestUserDetail({userId}).nick;
}

QString UserRepository::requestUserAvatarPath(const QString& userId) const
{
    return requestUserDetail({userId}).avatarPath;
}

QMap<QString, QString> UserRepository::requestFriendGroups() const
{
    QMutexLocker locker(&mutex);
    QMap<QString, QString> groups;
    for (const User& user : userMap) {
        if (!user.isFriend) {
            continue;
        }
        groups.insert(user.friendGroupId, user.friendGroupName);
    }
    return groups;
}

void UserRepository::saveUser(const User& user)
{
    bool changed = false;
    QString oldAvatarPath;
    QMutexLocker locker(&mutex);
    oldAvatarPath = userMap.value(user.id).avatarPath;
    changed = !userMap.contains(user.id) || userMap.value(user.id).nick != user.nick
            || userMap.value(user.id).remark != user.remark
            || userMap.value(user.id).avatarPath != user.avatarPath
            || userMap.value(user.id).status != user.status
            || userMap.value(user.id).signature != user.signature
            || userMap.value(user.id).isDnd != user.isDnd
            || userMap.value(user.id).isFriend != user.isFriend
            || userMap.value(user.id).friendGroupId != user.friendGroupId
            || userMap.value(user.id).friendGroupName != user.friendGroupName
            || userMap.value(user.id).region != user.region;
    userMap[user.id] = user;
    locker.unlock();

    if (!oldAvatarPath.isEmpty() && oldAvatarPath != user.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    if (changed) {
        emit friendListChanged();
    }
}

bool UserRepository::isFriend(const QString& userId) const
{
    QMutexLocker locker(&mutex);
    return userMap.contains(userId) && userMap.value(userId).isFriend;
}

void UserRepository::addFriend(const QString& userId,
                               const QString& groupId,
                               const QString& groupName)
{
    if (userId.isEmpty()) {
        return;
    }

    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = userMap.find(userId);
        if (it == userMap.end()) {
            return;
        }

        changed = !it->isFriend ||
                  it->friendGroupId != groupId ||
                  it->friendGroupName != groupName;
        it->isFriend = true;
        it->friendGroupId = groupId.isEmpty() ? QStringLiteral("default") : groupId;
        it->friendGroupName = groupName.isEmpty() ? QStringLiteral("默认分组") : groupName;
    }

    if (changed) {
        emit friendListChanged();
    }
}

void UserRepository::removeUser(const QString& userID)
{
    bool changed = false;
    {
        QMutexLocker locker(&mutex);
        auto it = userMap.find(userID);
        if (it != userMap.end() && it->isFriend) {
            it->isFriend = false;
            it->remark.clear();
            changed = true;
        }
    }
    if (changed) {
        emit friendListChanged();
    }
}

QString statusText(UserStatus userStatus)
{
    if (userStatus == Online) return "在线";
    if (userStatus == Offline) return "离线";
    if (userStatus == Flying) return "飞行模式";
    return "挖矿中";
}

QString statusIconPath(UserStatus userStatus)
{
    const QString prefix = ":/resources/icon/";
    if (userStatus == Online) return prefix + "online.png";
    if (userStatus == Offline) return prefix + "offline.png";
    if (userStatus == Flying) return prefix + "flying.png";
    return prefix + "mining.png";
}
