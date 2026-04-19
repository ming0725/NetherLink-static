#ifndef NETHERLINK_NOTIFICATIONMANAGER_H
#define NETHERLINK_NOTIFICATIONMANAGER_H

#include <QWidget>
#include <QLabel>
#include <QPropertyAnimation>

class NotificationManager : public QWidget
{
Q_OBJECT
public:
    enum Type {
        Success,
        Error
    };

    // 单例接口
    static NotificationManager& instance();

    // 全屏顶部居中版本
    void showMessage(const QString& message, Type type = Success);

    // 新增：在指定 QWidget 顶部弹出版本
    void showMessage(const QString& message, Type type, QWidget* targetWidget);

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    explicit NotificationManager(QWidget* parent = nullptr);
    ~NotificationManager() override = default;

    void setupUI();
    void startAnimation(bool show); // 保留给全屏居中版本使用

private:
    QLabel* iconLabel;
    QLabel* messageLabel;
    QPropertyAnimation* animation;
    bool isShowing = false;
};

#endif // NETHERLINK_NOTIFICATIONMANAGER_H
