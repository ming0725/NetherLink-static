#pragma once
#include <QColor>
#include <QFont>
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/StatefulPushButton.h"
#include "features/friend/ui/FriendListWidget.h"
#include "features/friend/ui/GroupListWidget.h"
#include "features/friend/ui/FriendDetailPage.h"
#include "features/friend/ui/GroupDetailPage.h"
#include "features/friend/ui/FriendNotificationPage.h"
#include "features/friend/ui/GroupNotificationPage.h"
#include "app/frame/DefaultPage.h"

class QPushButton;
class FriendSessionController;

class FriendApplication : public QWidget {
    Q_OBJECT
public:
    explicit FriendApplication(QWidget* parent = nullptr);

signals:
    void requestOpenConversation(const QString& conversationId);

protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr);
        FriendListWidget* friendList() const { return m_content; }
        GroupListWidget* groupList() const { return m_groupList; }
        IconLineEdit* searchInput() const { return m_searchInput; }

    protected:
        void resizeEvent(QResizeEvent* ev) override;

    private:
        void updateContentMode(bool animate = true);
        void applyTheme();

        IconLineEdit* m_searchInput;
        StatefulPushButton* m_addButton;
        QWidget* m_modeBar;
        QPushButton* m_friendModeButton;
        QPushButton* m_groupModeButton;
        FriendListWidget* m_content;
        GroupListWidget* m_groupList;
    };

    FriendSessionController* m_friendController;
    LeftPane*    m_leftPane;     // 左侧面板
    DefaultPage* m_defaultPage;  // 右侧默认页
    FriendDetailPage* m_detailPage;
    GroupDetailPage* m_groupDetailPage;
    FriendNotificationPage* m_notificationPage;
    GroupNotificationPage* m_groupNotificationPage;
    QStackedWidget* m_rightStack;
    QSplitter*   m_splitter;     // 中间分隔器

    void showNotificationPage();
    void hideNotificationPage();
    void populateNotificationData();
    void showGroupNotificationPage();
    void hideGroupNotificationPage();
    void populateGroupNotificationData();
};
