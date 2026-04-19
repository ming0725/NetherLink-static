#pragma once
#include "AiChatListWidget.h"
#include "DefaultPage.h"
#include <QSplitter>
#include <QWidget>

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
                , m_button(new QPushButton(this))
                , m_aiChatList(new AiChatListWidget(this))
        {
            setMinimumWidth(200);
            setMaximumWidth(400);
            m_aiChatList->setStyleSheet("border-width:0px;border-style:solid;");
            // 设置图标
            QPixmap pixmap(":/resources/icon/aichat.png");
            m_iconLabel->setPixmap(pixmap);
            m_iconLabel->setScaledContents(true); // 自动缩放
            m_iconLabel->setStyleSheet("background: transparent;");
            // 设置按钮
            m_button->setText("＋");
            m_button->setFixedSize(buttonSize * 1.6, buttonSize);
            m_button->setStyleSheet("border-radius:10px; background-color:#0078D7; color:white;");
            // 设置列表样式
            m_aiChatList->setStyleSheet("border-width:0px; border-style:solid;");
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
        QPushButton* m_button;
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



