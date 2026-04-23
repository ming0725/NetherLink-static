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
        explicit LeftPane(QWidget* parent = nullptr)
                : QWidget(parent)
                , m_searchInput(new IconLineEdit(this))
                , m_addButton(new StatefulPushButton("+", this))
                , m_content(new FriendListWidget(this))
        {
            setMinimumWidth(144);
            setMaximumWidth(305);
            m_searchInput->setFixedHeight(26);
            m_addButton->setRadius(8);
            m_addButton->setNormalColor(QColor(0xF5, 0xF5, 0xF5));
            m_addButton->setHoverColor(QColor(0xEB, 0xEB, 0xEB));
            m_addButton->setPressColor(QColor(0xD7, 0xD7, 0xD7));
            m_addButton->setTextColor(QColor(0x33, 0x33, 0x33));
            m_addButton->setFixedHeight(26);
            QFont addFont = m_addButton->font();
            addFont.setPixelSize(18);
            m_addButton->setFont(addFont);
            m_content->setStyleSheet("border-width:0px;border-style:solid;");
        }

    protected:
        void resizeEvent(QResizeEvent* ev) override {
            QWidget::resizeEvent(ev);
            const int topMargin = 24;
            const int bottomMargin = 12;
            const int leftMargin = 14;
            const int rightMargin = 14;
            const int spacing = 5;
            const int controlHeight = 26;
            const int buttonX = width() - rightMargin - controlHeight;

            m_addButton->setGeometry(buttonX, topMargin, controlHeight, controlHeight);
            m_searchInput->setGeometry(leftMargin,
                                       topMargin,
                                       qMax(0, buttonX - spacing - leftMargin),
                                       controlHeight);

            const int contentY = topMargin + controlHeight + bottomMargin;
            m_content->setGeometry(0, contentY, width(), qMax(0, height() - contentY));
        }

    private:
        IconLineEdit* m_searchInput;
        StatefulPushButton* m_addButton;
        FriendListWidget* m_content;
    };

    LeftPane*    m_leftPane;     // 左侧面板
    DefaultPage* m_defaultPage;  // 右侧默认页
    QSplitter*   m_splitter;     // 中间分隔器
};
