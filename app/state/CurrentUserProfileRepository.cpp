#include "CurrentUserProfileRepository.h"

#include "shared/data/RepositoryTemplate.h"
#include "shared/services/ImageService.h"

#include <utility>

namespace {

constexpr auto kDefaultAvatarPath = ":/resources/avatar/9.jpg";

CurrentUserProfile fallbackProfile(const QString& userId)
{
    CurrentUserProfile profile;
    profile.userId = userId;
    profile.nickName = userId.isEmpty() ? QStringLiteral("未登录用户") : userId;
    profile.avatarPath = QString::fromLatin1(kDefaultAvatarPath);
    profile.status = Offline;
    return profile;
}

CurrentUserProfile identityOnly(CurrentUserProfile profile)
{
    profile.signature.clear();
    profile.region.clear();
    return profile;
}

class CurrentUserIdentityRequestOperation final
    : public RepositoryTemplate<CurrentUserIdentityRequest, CurrentUserProfile> {
public:
    explicit CurrentUserIdentityRequestOperation(QMap<QString, CurrentUserProfile> profiles)
        : m_profiles(std::move(profiles))
    {
    }

private:
    CurrentUserProfile doRequest(const CurrentUserIdentityRequest& query) const override
    {
        return identityOnly(m_profiles.value(query.userId, fallbackProfile(query.userId)));
    }

    QMap<QString, CurrentUserProfile> m_profiles;
};

class CurrentUserProfileRequestOperation final
    : public RepositoryTemplate<CurrentUserProfileRequest, CurrentUserProfile> {
public:
    explicit CurrentUserProfileRequestOperation(QMap<QString, CurrentUserProfile> profiles)
        : m_profiles(std::move(profiles))
    {
    }

private:
    CurrentUserProfile doRequest(const CurrentUserProfileRequest& query) const override
    {
        return m_profiles.value(query.userId, fallbackProfile(query.userId));
    }

    QMap<QString, CurrentUserProfile> m_profiles;
};

} // namespace

CurrentUserProfileRepository::CurrentUserProfileRepository(QObject* parent)
    : QObject(parent)
{
    CurrentUserProfile profile;
    profile.userId = QStringLiteral("u007");
    profile.nickName = QStringLiteral("wbbb");
    profile.avatarPath = QString::fromLatin1(kDefaultAvatarPath);
    profile.status = Online;
    profile.signature = QStringLiteral("主城地基施工中，看到我在挖方块就是在线");
    profile.region = QStringLiteral("上海");
    m_profiles.insert(profile.userId, profile);
}

CurrentUserProfileRepository& CurrentUserProfileRepository::instance()
{
    static CurrentUserProfileRepository repo;
    return repo;
}

CurrentUserProfile CurrentUserProfileRepository::requestCurrentUserIdentity(
        const CurrentUserIdentityRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return CurrentUserIdentityRequestOperation(m_profiles).request(query);
}

CurrentUserProfile CurrentUserProfileRepository::requestCurrentUserProfile(
        const CurrentUserProfileRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    return CurrentUserProfileRequestOperation(m_profiles).request(query);
}

void CurrentUserProfileRepository::saveCurrentUserProfile(const CurrentUserProfile& profile)
{
    if (profile.userId.isEmpty()) {
        return;
    }

    bool changed = false;
    QString oldAvatarPath;
    {
        QMutexLocker locker(&m_mutex);
        const CurrentUserProfile previous = m_profiles.value(profile.userId);
        oldAvatarPath = previous.avatarPath;
        changed = previous.userId != profile.userId
                || previous.nickName != profile.nickName
                || previous.avatarPath != profile.avatarPath
                || previous.status != profile.status
                || previous.signature != profile.signature
                || previous.region != profile.region;
        m_profiles.insert(profile.userId, profile);
    }

    if (!oldAvatarPath.isEmpty() && oldAvatarPath != profile.avatarPath) {
        ImageService::instance().invalidateSource(oldAvatarPath);
    }
    if (changed) {
        emit currentUserProfileChanged(profile.userId);
    }
}
