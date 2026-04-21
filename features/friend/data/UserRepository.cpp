#include "UserRepository.h"

#include <QCollator>

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
                    user.isDnd
            });
        }
        return result;
    }

    void onAfterRequest(const FriendListRequest&, QVector<FriendSummary>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const FriendSummary& lhs, const FriendSummary& rhs) {
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
            {"u001", "momo", "", ":/resources/avatar/1.jpg", Online, "我是好momo"},
            {"u002", "blazer", "", ":/resources/avatar/0.jpg", Mining, "不掉烈焰棒"},
            {"u003", "不吃香菜（考研版）", "", ":/resources/avatar/3.jpg", Online, "绝不吃一口香菜！"},
            {"u004", "不抽香烟（一路菜花版）", "", ":/resources/avatar/5.jpg", Online, "芝士雪豹"},
            {"u005", "不会演戏（许仙版）", "", ":/resources/avatar/4.jpg", Offline, "不碍事的白姑娘"},
            {"u006", "TralaleroTralala", "", ":/resources/avatar/2.jpg", Online, "不穿Nike（蓝勾版）"},
            {"u007", "圆头耄耋", "", ":/resources/avatar/6.jpg", Offline, "这只猫很懒，什么都没留下来"},
            {"u008", "歪比巴卜", "", ":/resources/avatar/7.jpg", Mining, "歪比歪比"},
            {"u009", "tung tung tung sahur", "", ":/resources/avatar/8.jpg", Offline, "tung tung tung tung tung tung tung tung tung sahur"},
            {"u010", "BombardinoCrocodilo", "", ":/resources/avatar/9.jpg", Flying, "已塌房"},
    };

    for (const User& user : users) {
        userMap.insert(user.id, user);
    }
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
    QMutexLocker locker(&mutex);
    const QString oldAvatarPath = userMap.value(user.id).avatarPath;
    userMap[user.id] = user;
    if (!oldAvatarPath.isEmpty() && oldAvatarPath != user.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
}

void UserRepository::removeUser(const QString& userID)
{
    QMutexLocker locker(&mutex);
    userMap.remove(userID);
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
