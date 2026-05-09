#pragma once

#include <QString>

#include "shared/types/User.h"

struct CurrentUserProfile {
    QString userId;
    QString nickName;
    QString avatarPath;
    UserStatus status = Offline;
    QString signature;
    QString region;

    bool isValid() const { return !userId.isEmpty(); }
};
