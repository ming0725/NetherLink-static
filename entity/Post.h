#pragma once

#include <QString>
#include <QVector>

struct Post {
    QString postID;
    QString title;
    QString content;
    int likes;
    QString authorID;
    QVector<QString> picturesPath;
    bool isLiked = false;
};