#include "UserRepository.h"

#include <QCollator>
#include <QStringList>

#include "shared/services/ImageService.h"
#include "shared/data/RepositoryTemplate.h"

namespace {

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
            if (!query.keyword.isEmpty() &&
                !user.nick.contains(query.keyword, Qt::CaseInsensitive) &&
                !user.signature.contains(query.keyword, Qt::CaseInsensitive)) {
                continue;
            }

            result.push_back(FriendSummary{
                    user.id,
                    user.nick,
                    user.avatarPath,
                    user.status,
                    user.signature,
                    user.isDnd,
                    user.friendGroupId,
                    user.friendGroupName
            });
        }
        return result;
    }

    void onAfterRequest(const FriendListRequest&, QVector<FriendSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const FriendSummary& lhs, const FriendSummary& rhs) {
            if (lhs.groupName != rhs.groupName) {
                static QCollator groupCollator(QLocale::Chinese);
                groupCollator.setNumericMode(true);
                return groupCollator.compare(lhs.groupName, rhs.groupName) < 0;
            }

            if (lhs.status != Offline && rhs.status == Offline) {
                return true;
            }
            if (lhs.status == Offline && rhs.status != Offline) {
                return false;
            }

            static QCollator localCollator(QLocale::Chinese);
            localCollator.setNumericMode(true);
            return localCollator.compare(lhs.displayName, rhs.displayName) < 0;
        });
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
            {"u001", "momo", "", ":/resources/avatar/1.jpg", Online, "我是好momo", false, "g001", "常用联系人"},
            {"u002", "blazer", "", ":/resources/avatar/0.jpg", Mining, "不掉烈焰棒", false, "g001", "常用联系人"},
            {"u003", "不吃香菜（考研版）", "", ":/resources/avatar/3.jpg", Online, "绝不吃一口香菜！", false, "g001", "常用联系人"},
            {"u004", "不抽香烟（一路菜花版）", "", ":/resources/avatar/5.jpg", Online, "芝士雪豹", false, "g002", "同学"},
            {"u005", "不会演戏（许仙版）", "", ":/resources/avatar/4.jpg", Offline, "不碍事的白姑娘", false, "g002", "同学"},
            {"u006", "TralaleroTralala", "", ":/resources/avatar/2.jpg", Online, "不穿Nike（蓝勾版）", false, "g002", "同学"},
            {"u007", "圆头耄耋", "", ":/resources/avatar/6.jpg", Offline, "这只猫很懒，什么都没留下来", false, "g003", "服务器伙伴"},
            {"u008", "歪比巴卜", "", ":/resources/avatar/7.jpg", Mining, "歪比歪比", false, "g003", "服务器伙伴"},
            {"u009", "tung tung tung sahur", "", ":/resources/avatar/8.jpg", Offline, "tung tung tung tung tung tung tung tung tung sahur", false, "g003", "服务器伙伴"},
            {"u010", "BombardinoCrocodilo", "", ":/resources/avatar/9.jpg", Flying, "已塌房", false, "g003", "服务器伙伴"},
            {"u011", "momo", "", ":/resources/avatar/10.jpg", Online, "今天也要准时上线", false, "g004", "临时会话"},
            {"u012", "blazer", "", ":/resources/avatar/0.jpg", Offline, "刷怪塔维护中", false, "g004", "临时会话"},
            {"u013", "不吃香菜（考研版）", "", ":/resources/avatar/3.jpg", Mining, "背书到下界", false, "g004", "临时会话"},
            {"u014", "歪比巴卜", "", ":/resources/avatar/7.jpg", Online, "收到请回复", false, "g005", "最近添加"},
            {"u015", "圆头耄耋", "", ":/resources/avatar/6.jpg", Flying, "刚刚通过好友验证", false, "g005", "最近添加"},
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
                    "",
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
            || userMap.value(user.id).friendGroupName != user.friendGroupName;
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
