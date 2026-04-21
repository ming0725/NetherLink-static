#pragma once
#include <QString>
#include <QObject>

// 当前用户信息的全局单例类
class CurrentUser : public QObject {
    Q_OBJECT
public:
    static CurrentUser& instance();
    
    // 设置当前用户信息
    void setUserInfo(const QString& userId, const QString& userName = "", const QString& avatarPath = "");
    
    // 获取当前用户ID
    QString getUserId() const { return m_userId; }
    
    // 获取当前用户名称
    QString getUserName() const { return m_userName; }
    
    // 获取当前用户头像路径
    QString getAvatarPath() const { return m_avatarPath; }
    
    // 检查是否已设置用户
    bool isUserSet() const { return !m_userId.isEmpty(); }
    
    // 清除当前用户信息
    void clear();

    QWidget* getMainWindow() { return mainWindow; }

    void setMainWindow(QWidget* w) { mainWindow = w; }
    
private:
    explicit CurrentUser(QObject* parent = nullptr);
    Q_DISABLE_COPY(CurrentUser)
    
    QString m_userId;
    QString m_userName;
    QString m_avatarPath;
    QWidget* mainWindow;
}; 