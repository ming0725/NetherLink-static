// MessageApplication.cpp
#include "MessageApplication.h"
#include "MessageRepository.h"
#include "GroupRepository.h"
#include <QLayout>
#include <QResizeEvent>
#include <QPainter>

MessageApplication::MessageApplication(QWidget* parent)
        : QWidget(parent)
{
    // 左侧：搜索 + 列表
    QWidget* leftPane = new QWidget(this);
    leftPane->setMinimumWidth(144);
    leftPane->setMaximumWidth(305);

    m_topSearch = new TopSearchWidget(leftPane);
    m_msgList   = new MessageListWidget(leftPane);
    m_msgList->setStyleSheet("border-width:0px;border-style:solid;");
    connect(m_msgList, &MessageListWidget::itemClicked,
            this, &MessageApplication::onMessageClicked);

    QVBoxLayout* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(0,0,0,0);
    leftLayout->setSpacing(0);
    leftLayout->addWidget(m_topSearch);
    leftLayout->addWidget(m_msgList);
    leftLayout->setStretch(0, 0);
    leftLayout->setStretch(1, 1);

    // 右侧堆栈：初始页 + 聊天页
    m_rightStack  = new QStackedWidget(this);
    m_defaultPage = new DefaultPage(this);

    m_rightStack->addWidget(m_defaultPage);
    m_rightStack->setCurrentWidget(m_defaultPage);
    m_chatArea = new ChatArea(this);
    m_rightStack->addWidget(m_chatArea);

    // 整体分割
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(leftPane);
    m_splitter->addWidget(m_rightStack);
    m_splitter->setHandleWidth(0);
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({200, width() - 200});
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    // 主布局
    QHBoxLayout* mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(0,0,0,0);
    mainLayout->addWidget(m_splitter);

    setWindowFlag(Qt::FramelessWindowHint);
    connect(m_chatArea, &ChatArea::sendMessage, m_msgList, &MessageListWidget::onLastMessageUpdated);
}

void MessageApplication::resizeEvent(QResizeEvent*)
{
    m_splitter->setGeometry(rect());
}

void MessageApplication::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(Qt::white);
    p.drawRect(rect());
}

void MessageApplication::onMessageClicked(MessageListItem *item)
{
    if (!item)
        return;
    m_chatArea->clearAll();
    m_rightStack->setCurrentWidget(m_chatArea);
    auto& mr = MessageRepository::instance();
    auto& gr = GroupRepository::instance();
    auto msgs = mr.getMessages(item->getChatID());
    auto id = item->getChatID();
    bool isGroup = gr.isGroup(id);
    m_chatArea->setGroupMode(isGroup);
    m_chatArea->setMessageId(id);
    m_chatArea->initMessage(msgs);
}
