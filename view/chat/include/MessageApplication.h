#pragma once
#include <QWidget>
#include <QSplitter>
#include <QStackedWidget>
#include "TopSearchWidget.h"
#include "MessageListWidget.h"
#include "DefaultPage.h"
#include "ChatArea.h"

class MessageApplication : public QWidget {
    Q_OBJECT
public:
    explicit MessageApplication(QWidget* parent = nullptr);
protected:
    void resizeEvent(QResizeEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
private slots:
    void onMessageClicked(MessageListItem* item);
private:
    QSplitter*          m_splitter;
    TopSearchWidget*    m_topSearch;
    MessageListWidget*  m_msgList;
    QStackedWidget*     m_rightStack;
    DefaultPage*        m_defaultPage;
    ChatArea*           m_chatArea;
};
