#pragma once

#include <QString>
#include <QVector>

struct Group {
    QString groupId;
    QString groupName;
    int memberNum;
    QString ownerId;
    QString groupAvatarPath;
    bool isDnd = false;
    QVector<QString> adminsID;
    QString remark;
    QVector<QString> membersID;
};