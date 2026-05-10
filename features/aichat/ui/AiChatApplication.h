#pragma once
#include "AiChatListWidget.h"
#include "features/aichat/ui/AiChatConversationWidget.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/theme/ThemeManager.h"
#include <QSplitter>
#include <QWidget>
#include <QLabel>
#include <QPainter>
#include <QPaintEvent>

class AiChatApplication : public QWidget {
    Q_OBJECT
public:
    explicit AiChatApplication(QWidget* parent = nullptr);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private:
    class LeftPane : public QWidget {
    public:
        explicit LeftPane(QWidget* parent = nullptr)
                : QWidget(parent)
                , m_iconLabel(new QLabel(this))
                , m_newConversationButton(new StatefulPushButton(QStringLiteral("新对话"), this))
                , m_aiChatList(new AiChatListWidget(this))
        {
            setMinimumWidth(200);
            setMaximumWidth(400);
            // 设置图标
            QPixmap pixmap(":/resources/icon/aichat.png");
            m_iconLabel->setPixmap(pixmap);
            m_iconLabel->setScaledContents(true); // 自动缩放
            // 设置按钮
            m_newConversationButton->setFixedSize(newButtonWidth, newButtonHeight);
            m_newConversationButton->setRadius(6);
            connect(m_newConversationButton, &StatefulPushButton::clicked,
                    m_aiChatList, &AiChatListWidget::createNewConversation);
            applyTheme();
            connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
                applyTheme();
            });
        }
        AiChatListWidget* aiChatList() const { return m_aiChatList; }
    protected:
        void paintEvent(QPaintEvent* event) override {
            QPainter painter(this);
            painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PanelBackground));
        }

        void resizeEvent(QResizeEvent* ev) override {
            QWidget::resizeEvent(ev);
            int y = topMargin;
            QPixmap originalPixmap(":/resources/icon/aichat.png");
            QSize iconTargetSize(iconSize, iconSize);
            if (!originalPixmap.isNull()) {
                QSize scaledSize = originalPixmap.size();
                scaledSize.scale(iconSize, iconSize, Qt::KeepAspectRatio);
                iconTargetSize = scaledSize;
            }
            m_iconLabel->setGeometry(leftMargin, y, iconTargetSize.width(), iconTargetSize.height());
            m_newConversationButton->move(qMax(leftMargin + iconTargetSize.width() + headerSpacing,
                                               width() - rightMargin - newButtonWidth),
                                          y + (iconTargetSize.height() - newButtonHeight) / 2);
            y += iconTargetSize.height() + headerListDist;
            m_aiChatList->setGeometry(0, y, width(), height() - y);
        }
    private:
        void applyTheme() {
            m_newConversationButton->setNormalColor(ThemeManager::instance().color(ThemeColor::Accent));
            m_newConversationButton->setHoverColor(ThemeManager::instance().color(ThemeColor::AccentHover));
            m_newConversationButton->setPressColor(ThemeManager::instance().color(ThemeColor::AccentPressed));
            m_newConversationButton->setTextColor(Qt::white);
            update();
        }

        AiChatListWidget* m_aiChatList;
        QLabel* m_iconLabel;
        StatefulPushButton* m_newConversationButton;
        const int topMargin = 20;
        const int leftMargin = 10;
        const int rightMargin = 14;
        const int iconSize = 50;
        const int headerSpacing = 10;
        const int headerListDist = 10;
        const int newButtonWidth = 82;
        const int newButtonHeight = 30;
    };
    LeftPane*     m_leftPane;
    AiChatConversationWidget* m_conversationWidget;
    QSplitter*    m_splitter;
};
