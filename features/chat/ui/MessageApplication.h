#pragma once
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include "shared/ui/TopSearchWidget.h"
#include "features/chat/ui/MessageListWidget.h"
#include "app/frame/DefaultPage.h"
#include "features/chat/ui/ChatArea.h"

class MessageApplication : public QWidget {
    Q_OBJECT
public:
    explicit MessageApplication(QWidget* parent = nullptr);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private slots:
    void onMessageClicked(const QString& conversationId);
private:
    void ensureChatArea();
    QSplitter*          m_splitter;
    TopSearchWidget*    m_topSearch;
    MessageListWidget*  m_msgList;
    QStackedWidget*     m_rightStack;
    DefaultPage*        m_defaultPage;
    ChatArea*           m_chatArea = nullptr;
};
