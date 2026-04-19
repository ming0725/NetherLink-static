#pragma once

#include <QDateTime>
#include <QString>
#include <QVector>

struct Post {
    QString postID;
    QString title;
    QString content;
    int likes;
    int commentCount = 0;
    QString authorID;
    QDateTime createdAt;
    QVector<QString> picturesPath;
    bool isLiked = false;
};
