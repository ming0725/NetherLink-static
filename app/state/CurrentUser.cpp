#include "CurrentUser.h"

#include "CurrentUserProfileRepository.h"

CurrentUser::CurrentUser(QObject* parent)
    : QObject(parent)
{
    m_userId = QStringLiteral("u007");
    refreshIdentity();
    connect(&CurrentUserProfileRepository::instance(),
            &CurrentUserProfileRepository::currentUserProfileChanged,
            this,
            [this](const QString& userId) {
                if (isCurrentUserId(userId)) {
                    if (m_profileLoadLevel == ProfileLoadLevel::Full) {
                        refreshProfile();
                    } else {
                        refreshIdentity();
                    }
                }
            });
}

CurrentUser& CurrentUser::instance()
{
    static CurrentUser instance;
    return instance;
}

void CurrentUser::setUserInfo(const QString& userId, const QString& userName, const QString& avatarPath)
{
    m_userId = userId;
    m_profile = {};
    m_profileLoadLevel = ProfileLoadLevel::None;

    if (m_userId.isEmpty()) {
        emit identityChanged();
        emit profileChanged();
        return;
    }

    refreshIdentity();
    if (!userName.isEmpty()) {
        m_profile.nickName = userName;
    }
    if (!avatarPath.isEmpty()) {
        m_profile.avatarPath = avatarPath;
    }
    if (!userName.isEmpty() || !avatarPath.isEmpty()) {
        refreshProfile();
        if (!userName.isEmpty()) {
            m_profile.nickName = userName;
        }
        if (!avatarPath.isEmpty()) {
            m_profile.avatarPath = avatarPath;
        }
        CurrentUserProfileRepository::instance().saveCurrentUserProfile(m_profile);
        emit identityChanged();
        emit profileChanged();
    }
}

QString CurrentUser::getUserName() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.nickName;
}

QString CurrentUser::getAvatarPath() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.avatarPath;
}

UserStatus CurrentUser::getStatus() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile.status;
}

QString CurrentUser::getSignature() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile.signature;
}

QString CurrentUser::getRegion() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile.region;
}

CurrentUserProfile CurrentUser::identity() const
{
    ensureProfileLoaded(ProfileLoadLevel::Identity);
    return m_profile;
}

CurrentUserProfile CurrentUser::profile() const
{
    ensureProfileLoaded(ProfileLoadLevel::Full);
    return m_profile;
}

void CurrentUser::refreshIdentity()
{
    if (m_userId.isEmpty()) {
        return;
    }
    applyProfile(CurrentUserProfileRepository::instance().requestCurrentUserIdentity({m_userId}),
                 ProfileLoadLevel::Identity);
}

void CurrentUser::refreshProfile()
{
    if (m_userId.isEmpty()) {
        return;
    }
    applyProfile(CurrentUserProfileRepository::instance().requestCurrentUserProfile({m_userId}),
                 ProfileLoadLevel::Full);
}

void CurrentUser::saveProfile(const CurrentUserProfile& profile)
{
    if (m_userId.isEmpty() || profile.userId != m_userId) {
        return;
    }
    CurrentUserProfileRepository::instance().saveCurrentUserProfile(profile);
    applyProfile(profile, ProfileLoadLevel::Full);
}

void CurrentUser::clear()
{
    m_userId.clear();
    m_profile = {};
    m_profileLoadLevel = ProfileLoadLevel::None;
    emit identityChanged();
    emit profileChanged();
}

void CurrentUser::ensureProfileLoaded(ProfileLoadLevel level) const
{
    if (m_userId.isEmpty() ||
        m_profileLoadLevel == ProfileLoadLevel::Full ||
        m_profileLoadLevel == level) {
        return;
    }

    CurrentUser* self = const_cast<CurrentUser*>(this);
    if (level == ProfileLoadLevel::Full) {
        self->refreshProfile();
    } else {
        self->refreshIdentity();
    }
}

void CurrentUser::applyProfile(CurrentUserProfile profile, ProfileLoadLevel level)
{
    if (profile.userId.isEmpty()) {
        profile.userId = m_userId;
    }

    ProfileLoadLevel effectiveLevel = level;
    if (level == ProfileLoadLevel::Identity &&
        m_profileLoadLevel == ProfileLoadLevel::Full &&
        m_profile.userId == profile.userId) {
        profile.signature = m_profile.signature;
        profile.region = m_profile.region;
        effectiveLevel = ProfileLoadLevel::Full;
    }

    const bool identityChangedValue = m_profile.userId != profile.userId
            || m_profile.nickName != profile.nickName
            || m_profile.avatarPath != profile.avatarPath
            || m_profile.status != profile.status;
    const bool fullChangedValue = identityChangedValue
            || m_profile.signature != profile.signature
            || m_profile.region != profile.region
            || m_profileLoadLevel != effectiveLevel;

    m_profile = profile;
    m_profileLoadLevel = effectiveLevel;

    if (identityChangedValue) {
        emit identityChanged();
    }
    if (fullChangedValue) {
        emit profileChanged();
    }
}
