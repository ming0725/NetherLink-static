#pragma once

#include <QWidget>

#include "shared/types/User.h"

class QLabel;
class QLineEdit;
class StatefulPushButton;
class QToolButton;
class QVBoxLayout;
class QWidget;

class FriendDetailPage : public QWidget
{
    Q_OBJECT

public:
    explicit FriendDetailPage(QWidget* parent = nullptr);
    ~FriendDetailPage() override;

public slots:
    void setUserId(const QString& userId);
    void clear();

signals:
    void requestMessage(const QString& userId);
    void friendDeleted();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    class AvatarLabel;

    void setUser(const User& user);
    void updateAvatar();
    void updateRemarkText();
    void updateGroupButtonText();
    void updateSignatureText();
    void saveRemark();
    void rebuildGroupMenu();
    void changeGroup(const QString& groupId, const QString& groupName);
    void confirmDeleteFriend();

    User m_user;
    bool m_hasUser = false;

    AvatarLabel* m_avatarLabel;
    QLabel* m_nameLabel;
    QLabel* m_idLabel;
    QWidget* m_regionRow;
    QLabel* m_regionLabel;
    QLabel* m_statusIcon;
    QLabel* m_statusLabel;
    QLineEdit* m_remarkEdit;
    QToolButton* m_groupButton;
    QLabel* m_signatureLabel;
    StatefulPushButton* m_messageButton;
    StatefulPushButton* m_deleteButton;
};
