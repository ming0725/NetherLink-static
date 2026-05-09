#pragma once

#include <QWidget>

#include "shared/types/User.h"

class QLabel;
class PaintedLabel;
class InlineEditableText;
class FriendSessionController;
class StatefulPushButton;
class StyledActionMenu;
class QToolButton;
class QVBoxLayout;
class QWidget;

class FriendDetailPage : public QWidget
{
    Q_OBJECT

public:
    explicit FriendDetailPage(QWidget* parent = nullptr);
    ~FriendDetailPage() override;
    void setController(FriendSessionController* controller);

public slots:
    void setUserId(const QString& userId);
    void clear();

signals:
    void requestMessage(const QString& userId);
    void friendDeleted();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setUser(const User& user);
    void updateAvatar();
    void applyTheme();
    void updateRemarkText();
    void updateGroupButtonText();
    void updateSignatureText();
    void saveRemark();
    void showGroupMenu();
    void rebuildGroupMenu();
    void changeGroup(const QString& groupId, const QString& groupName);
    void confirmDeleteFriend();

    User m_user;
    bool m_hasUser = false;
    FriendSessionController* m_controller = nullptr;

    QString m_avatarSource;
    QWidget* m_contentWidget;
    PaintedLabel* m_nameLabel;
    PaintedLabel* m_idLabel;
    QWidget* m_regionRow;
    PaintedLabel* m_regionLabel;
    QLabel* m_statusIcon;
    PaintedLabel* m_statusLabel;
    InlineEditableText* m_remarkEdit;
    QToolButton* m_groupButton;
    StyledActionMenu* m_groupMenu;
    PaintedLabel* m_signatureLabel;
    StatefulPushButton* m_messageButton;
    StatefulPushButton* m_deleteButton;
};
