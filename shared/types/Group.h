#pragma once

#include <QMap>
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
    QString introduction;
    QString announcement;
    QString currentUserNickname;
    QMap<QString, QString> memberNicknames;
    QVector<QString> membersID;
    QString listGroupId = "gg_joined";
    QString listGroupName = "我加入的群聊";
};
