#pragma once

#include <QObject>
#include <QString>

#include "CurrentUserProfile.h"

class CurrentUser : public QObject {
    Q_OBJECT
public:
    static CurrentUser& instance();

    void setUserInfo(const QString& userId, const QString& userName = "", const QString& avatarPath = "");

    QString getUserId() const { return m_userId; }
    QString getUserName() const;
    QString getAvatarPath() const;
    UserStatus getStatus() const;
    QString getSignature() const;
    QString getRegion() const;
    CurrentUserProfile identity() const;
    CurrentUserProfile profile() const;
    bool isUserSet() const { return !m_userId.isEmpty(); }
    bool isCurrentUserId(const QString& userId) const { return !m_userId.isEmpty() && userId == m_userId; }
    void refreshIdentity();
    void refreshProfile();
    void saveProfile(const CurrentUserProfile& profile);
    void clear();

signals:
    void identityChanged();
    void profileChanged();

private:
    enum class ProfileLoadLevel {
        None,
        Identity,
        Full
    };

    explicit CurrentUser(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUser)

    void ensureProfileLoaded(ProfileLoadLevel level) const;
    void applyProfile(CurrentUserProfile profile, ProfileLoadLevel level);

    QString m_userId;
    mutable CurrentUserProfile m_profile;
    mutable ProfileLoadLevel m_profileLoadLevel = ProfileLoadLevel::None;
};
