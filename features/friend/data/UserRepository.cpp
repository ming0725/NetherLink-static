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
            if (!matchesFriendKeyword(user, query.keyword)) {
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
            if (!matchesFriendKeyword(user, query.keyword)) {
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
            if (normalizedFriendGroupId(user) != query.groupId ||
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
    const QVector<User> users = {
            {"u001", "momo", "小桃", ":/resources/avatar/1.jpg", Online, "我是好momo", false, "g001", "常用联系人", "上海"},
            {"u002", "blazer", "", ":/resources/avatar/0.jpg", Mining, "不掉烈焰棒", false, "g001", "常用联系人", "下界要塞"},
            {"u003", "不吃香菜（考研版）", "香菜克星", ":/resources/avatar/3.jpg", Online, "绝不吃一口香菜！", false, "g001", "常用联系人", "北京"},
            {"u004", "不抽香烟（一路菜花版）", "", ":/resources/avatar/5.jpg", Online, "芝士雪豹", false, "g002", "同学", "杭州"},
            {"u005", "不会演戏（许仙版）", "许仙", ":/resources/avatar/4.jpg", Offline, "不碍事的白姑娘", false, "g002", "同学", "苏州"},
            {"u006", "TralaleroTralala", "", ":/resources/avatar/2.jpg", Online, "不穿Nike（蓝勾版）", false, "g002", "同学"},
            {"u007", "圆头耄耋", "", ":/resources/avatar/6.jpg", Offline, "这只猫很懒，什么都没留下来", false, "g003", "服务器伙伴"},
            {"u008", "歪比巴卜", "指令哥", ":/resources/avatar/7.jpg", Mining, "歪比歪比", false, "g003", "服务器伙伴"},
            {"u009", "tung tung tung sahur", "", ":/resources/avatar/8.jpg", Offline, "tung tung tung tung tung tung tung tung tung sahur", false, "g003", "服务器伙伴"},
            {"u010", "BombardinoCrocodilo", "", ":/resources/avatar/9.jpg", Flying, "已塌房", false, "g003", "服务器伙伴"},
            {"u011", "momo", "临时 momo", ":/resources/avatar/10.jpg", Online, "今天也要准时上线", false, "g004", "临时会话"},
            {"u012", "blazer", "", ":/resources/avatar/0.jpg", Offline, "刷怪塔维护中", false, "g004", "临时会话"},
            {"u013", "不吃香菜（考研版）", "", ":/resources/avatar/3.jpg", Mining, "背书到下界", false, "g004", "临时会话"},
            {"u014", "歪比巴卜", "", ":/resources/avatar/7.jpg", Online, "收到请回复", false, "g005", "最近添加"},
            {"u015", "圆头耄耋", "新好友", ":/resources/avatar/6.jpg", Flying, "刚刚通过好友验证", false, "g005", "最近添加"},
            {"u016", "TralaleroTralala", "", ":/resources/avatar/2.jpg", Offline, "晚点再聊", false, "g005", "最近添加"},
    };

    for (const User& user : users) {
        userMap.insert(user.id, user);
    }

    auto addPerformanceGroup = [this](const QString& groupId,
                                      const QString& groupName,
                                      int count,
                                      int idOffset) {
        const QStringList names = {
                "momo",
                "blazer",
                "不吃香菜（考研版）",
                "歪比巴卜",
                "圆头耄耋",
                "TralaleroTralala",
                "BombardinoCrocodilo",
                "不抽香烟（一路菜花版）"
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
                    (index % 9 == 0) ? QString("备注好友 %1").arg(index + 1) : QString(),
                    QString(":/resources/avatar/%1.jpg").arg(index % 11),
                    statuses.at(index % statuses.size()),
                    QString("性能测试好友 %1").arg(index + 1),
                    false,
                    groupId,
                    groupName
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

void UserRepository::removeUser(const QString& userID)
{
    bool removed = false;
    QMutexLocker locker(&mutex);
    removed = userMap.remove(userID) > 0;
    locker.unlock();
    if (removed) {
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
