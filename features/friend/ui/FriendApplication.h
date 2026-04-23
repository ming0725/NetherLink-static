#pragma once
#include <QColor>
#include <QFont>
#include <QWidget>
#include <QSplitter>
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/StatefulPushButton.h"
#include "features/friend/ui/FriendListWidget.h"
#include "app/frame/DefaultPage.h"

class FriendApplication : public QWidget {
    Q_OBJECT
public:
    explicit FriendApplication(QWidget* parent = nullptr);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr);
        FriendListWidget* friendList() const { return m_content; }
        IconLineEdit* searchInput() const { return m_searchInput; }

    protected:
        void resizeEvent(QResizeEvent* ev) override;

    private:
        IconLineEdit* m_searchInput;
        StatefulPushButton* m_addButton;
        FriendListWidget* m_content;
    };

    LeftPane*    m_leftPane;     // 左侧面板
    DefaultPage* m_defaultPage;  // 右侧默认页
    QSplitter*   m_splitter;     // 中间分隔器
};
