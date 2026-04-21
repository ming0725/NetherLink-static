#pragma once
#include <QString>

enum UserStatus {
    Online,
    Offline,
    Mining,
    Flying
};

struct User {
    QString id;
    QString nick;
    QString remark;
    QString avatarPath;
    UserStatus status;
    QString signature;
    bool isDnd = false;
};

QString statusText(UserStatus userStatus);
QString statusIconPath(UserStatus userStatus);