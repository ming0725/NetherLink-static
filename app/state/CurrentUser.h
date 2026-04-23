#pragma once
#include <QString>
#include <QObject>

class CurrentUser : public QObject {
    Q_OBJECT
public:
    static CurrentUser& instance();
    
    void setUserInfo(const QString& userId, const QString& userName = "", const QString& avatarPath = "");

    QString getUserId() const { return m_userId; }
    QString getUserName() const { return m_userName; }
    QString getAvatarPath() const { return m_avatarPath; }
    bool isUserSet() const { return !m_userId.isEmpty(); }
    void clear();
    
private:
    explicit CurrentUser(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUser)
    
    QString m_userId;
    QString m_userName;
    QString m_avatarPath;
};
