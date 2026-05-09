#pragma once

#include <QWidget>

#include "shared/types/Group.h"

class QLabel;
class PaintedLabel;
class InlineEditableText;
class FriendSessionController;
class StatefulPushButton;
class StyledActionMenu;
class QToolButton;

class GroupDetailPage : public QWidget
{
    Q_OBJECT

public:
    explicit GroupDetailPage(QWidget* parent = nullptr);
    ~GroupDetailPage() override;
    void setController(FriendSessionController* controller);

public slots:
    void setGroupId(const QString& groupId);
    void clear();

signals:
    void requestMessage(const QString& groupId);
    void groupExited();

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private:
    void setGroup(const Group& group);
    void updateAvatar();
    void applyTheme();
    void updateRemarkText();
    void updateCategoryButtonText();
    void updateIntroText();
    void updateAnnouncementText();
    void updateMemberCountText();
    QString elidedValueText(const QString& text, const QLabel* label) const;
    void saveRemark();
    void showCategoryMenu();
    void rebuildCategoryMenu();
    void changeCategory(const QString& categoryId, const QString& categoryName);
    void updateExitButtonState();
    void confirmExitGroup();

    Group m_group;
    bool m_hasGroup = false;
    FriendSessionController* m_controller = nullptr;

    QString m_avatarSource;
    QWidget* m_contentWidget;
    PaintedLabel* m_nameLabel;
    PaintedLabel* m_idLabel;
    InlineEditableText* m_remarkEdit;
    QToolButton* m_categoryButton;
    StyledActionMenu* m_categoryMenu;
    PaintedLabel* m_introLabel;
    PaintedLabel* m_announcementLabel;
    PaintedLabel* m_memberCountLabel;
    StatefulPushButton* m_messageButton;
    StatefulPushButton* m_exitButton;
};
