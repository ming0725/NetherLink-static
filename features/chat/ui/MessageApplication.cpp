// MessageApplication.cpp
#include "MessageApplication.h"
#include "features/chat/data/MessageRepository.h"

#include <QPainter>
#include <QResizeEvent>

MessageApplication::LeftPane::LeftPane(QWidget* parent)
        : QWidget(parent)
        , m_searchInput(new IconLineEdit(this))
        , m_addButton(new StatefulPushButton("+", this))
        , m_msgList(new MessageListWidget(this))
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

    m_msgList->setStyleSheet("border-width:0px;border-style:solid;");
}

void MessageApplication::LeftPane::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

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
    m_msgList->setGeometry(0, contentY, width(), qMax(0, height() - contentY));
}

MessageApplication::MessageApplication(QWidget* parent)
        : QWidget(parent)
{
    m_leftPane = new LeftPane(this);
    m_msgList = m_leftPane->messageList();
    connect(m_msgList, &MessageListWidget::conversationActivated,
            this, &MessageApplication::onMessageClicked);

    // 右侧堆栈：初始页 + 聊天页
    m_rightStack  = new QStackedWidget(this);
    m_defaultPage = new DefaultPage(this);

    m_rightStack->addWidget(m_defaultPage);
    m_rightStack->setCurrentWidget(m_defaultPage);

    // 整体分割
    m_splitter = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_rightStack);
    m_splitter->setHandleWidth(0);
    m_splitter->setStyleSheet("QSplitter::handle { background: transparent; border: none; }");
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 0);
    m_splitter->setStretchFactor(1, 1);
    m_splitter->setSizes({200, width() - 200});
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    setWindowFlag(Qt::FramelessWindowHint);
    m_msgList->clearCurrentConversationSelection();
    m_rightStack->setCurrentWidget(m_defaultPage);
}

void MessageApplication::handleGlobalMousePress(const QPoint& globalPos)
{
    if (m_chatArea && m_rightStack->currentWidget() == m_chatArea) {
        m_chatArea->handleGlobalMousePress(globalPos);
    }
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

void MessageApplication::onMessageClicked(const QString& conversationId)
{
    if (conversationId.isEmpty())
        return;
    ensureChatArea();
    m_chatArea->clearAll();
    m_rightStack->setCurrentWidget(m_chatArea);
    auto& mr = MessageRepository::instance();
    auto msgs = mr.requestConversationMessages({conversationId});
    auto meta = mr.requestConversationMeta({conversationId});
    m_chatArea->setConversationMeta(meta);
    m_chatArea->setMessageId(conversationId);
    m_chatArea->initMessage(msgs);
}

void MessageApplication::ensureChatArea()
{
    if (m_chatArea) {
        return;
    }

    m_chatArea = new ChatArea(this);
    m_rightStack->addWidget(m_chatArea);
    connect(m_chatArea, &ChatArea::sendMessage,
            m_msgList, &MessageListWidget::onLastMessageUpdated);
}
