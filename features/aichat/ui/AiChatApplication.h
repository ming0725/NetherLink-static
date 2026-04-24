#pragma once
#include "AiChatListWidget.h"
#include "app/frame/DefaultPage.h"
#include "shared/ui/StatefulPushButton.h"
#include <QSplitter>
#include <QWidget>
#include <QLabel>

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
                , m_button(new StatefulPushButton("+", this))
                , m_aiChatList(new AiChatListWidget(this))
        {
            setMinimumWidth(200);
            setMaximumWidth(400);
            // 设置图标
            QPixmap pixmap(":/resources/icon/aichat.png");
            m_iconLabel->setPixmap(pixmap);
            m_iconLabel->setScaledContents(true); // 自动缩放
            // 设置按钮
            m_button->setFixedSize(buttonSize * 1.6, buttonSize);
            m_button->setRadius(10);
            m_button->setNormalColor(QColor(0x00, 0x78, 0xD7));
            m_button->setHoverColor(QColor(0x10, 0x86, 0xE0));
            m_button->setPressColor(QColor(0x00, 0x6B, 0xC2));
            m_button->setTextColor(Qt::white);
        }
    protected:
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
            y += iconTargetSize.height() + iconBtnDist;
            m_button->move(leftMargin + 10, y);
            y += buttonSize + btnListDist;
            m_aiChatList->setGeometry(0, y, width(), height() - y);
        }
    private:
        AiChatListWidget* m_aiChatList;
        QLabel* m_iconLabel;
        StatefulPushButton* m_button;
        const int topMargin = 20;
        const int leftMargin = 10;
        const int iconSize = 50;
        const int iconBtnDist = 10;
        const int btnListDist = 2;
        const int buttonSize = 20;
    };
    LeftPane*     m_leftPane;
    DefaultPage*  m_defaultPage;
    QSplitter*    m_splitter;
};

