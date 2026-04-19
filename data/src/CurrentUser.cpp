#include "CurrentUser.h"
#include <QDebug>

CurrentUser::CurrentUser(QObject* parent)
    : QObject(parent)
{
    qDebug() << "CurrentUser initialized";
    m_userId = "u007";
    m_userName = "圆头耄耋";
    m_avatarPath = ":/resources/avatar/6.jpg";
}

CurrentUser& CurrentUser::instance()
{
    static CurrentUser instance;
    return instance;
}

void CurrentUser::setUserInfo(const QString& userId, const QString& userName, const QString& avatarPath)
{

    m_userId = userId;
    if (!userName.isEmpty()) {
        m_userName = userName;
    }

    if (!avatarPath.isEmpty()) {
        m_avatarPath = avatarPath;
    }

}

void CurrentUser::clear()
{
    m_userId = "";
    m_userName = "";
    m_avatarPath = "";
}
