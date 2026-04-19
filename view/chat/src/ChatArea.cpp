#include "ChatArea.h"
#include "TopSearchWidget.h"
#include "GroupRepository.h"
#include "UserRepository.h"
#include "MessageRepository.h"
#include "CurrentUser.h"
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QResizeEvent>
#include <QDateTime>

ChatArea::ChatArea(QWidget *parent)
        : QWidget(parent)
        , unreadMessageCount(0)
        , isAtBottom(true)
        , isGroupMode(false)
{
    // 创建布局
    QVBoxLayout* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);

    // 创建视图组件
    chatView = new ChatListView(this);
    chatView->setSpacing(2);  // 设置消息间距
    
    // 创建模型和代理
    chatModel = new ChatListModel(this);
    chatDelegate = new ChatItemDelegate(this);
    
    // 设置模型和代理
    chatView->setModel(chatModel);
    chatView->setItemDelegate(chatDelegate);
    
    // 创建新消息提示组件
    newMessageNotifier = new NewMessageNotifier(this);
    newMessageNotifier->hide();

    // 创建悬浮输入栏
    inputBar = new FloatingInputBar(this);
    inputBar->show();

    QWidget* chatInfo = new QWidget(this);
    chatInfo->setFixedHeight(24 + 26 + 12);
    chatInfo->setObjectName("ChatInfo");
    chatInfo->setStyleSheet("#ChatInfo { background-color: #F2F2F2; border-bottom: 1px solid #E9E9E9; }");

    // 外层垂直布局：用于将内容推到底部
    QVBoxLayout* outerLayout = new QVBoxLayout(chatInfo);
    outerLayout->setContentsMargins(20, 5, 5, 10); // 留出左下边距
    outerLayout->addStretch(); // 将内容推到底部

    // 内层水平布局：图标 + 名字
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(5); // 图标与文字间距
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 在线状态图标
    statusIcon = new QLabel(chatInfo);

    // 名字 Label
    nameLabel = new QLabel("", chatInfo);
    nameLabel->setStyleSheet("font-size: 17px; color: #000000;");

    // 添加到底部布局
    bottomLayout->addWidget(nameLabel);
    bottomLayout->addWidget(statusIcon);
    bottomLayout->addStretch(); // 保证靠左

    // 添加到底部区域
    outerLayout->addLayout(bottomLayout);

    chatInfo->setLayout(outerLayout);
    
    // 添加到主布局
    mainLayout->addWidget(chatInfo);
    mainLayout->addWidget(chatView);
    setLayout(mainLayout);

    // 连接信号槽
    connect(chatView->verticalScrollBar(), &QScrollBar::valueChanged,
            this, &ChatArea::onScrollValueChanged);
    connect(newMessageNotifier, &NewMessageNotifier::clicked,
            this, &ChatArea::onNewMessageNotifierClicked);
    connect(inputBar, &FloatingInputBar::sendImage,
            this, &ChatArea::onSendImage);
    connect(inputBar, &FloatingInputBar::sendText,
            this, &ChatArea::onSendText);

    // 设置样式
    chatView->setStyleSheet(
        "QListView {"
        "    background-color: #F2F2F2;"
        "}"
    );
}


void ChatArea::addMessage(QSharedPointer<ChatMessage> message)
{
    bool shouldScroll = message->isFromMe() || isNearBottom();
    // 添加消息
    chatModel->addMessage(message);
    MessageRepository::instance().addMessage(messageId, message);
    emit sendMessage(messageId, message->getContent(), message->getTimestamp());

    // 调整底部空白
    adjustBottomSpace();
    
    // 根据情况处理滚动和未读计数
    if (!message->isFromMe() && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }
    
    // 如果需要滚动到底部，使用延时确保布局更新完成
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}


void ChatArea::addImageMessage(QSharedPointer<ImageMessage> message,
                               const QDateTime& timestamp)
{
    bool isFromMe = message->isFromMe();
    bool shouldScroll = isFromMe || isNearBottom();
    // 设置消息时间戳为当前时间
    message->setTimestamp(timestamp);
    // 添加消息
    chatModel->addMessage(std::move(message));
    
    // 根据情况处理滚动和未读计数
    if (!isFromMe && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }
    
    // 最后处理滚动，确保新消息完全可见
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}

void ChatArea::onScrollValueChanged(int)
{
    isAtBottom = isScrollAtBottom();
    
    if (isAtBottom) {
        unreadMessageCount = 0;
        updateNewMessageNotifier();
    }
}



bool ChatArea::isNearBottom() const
{
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    int totalHeight = scrollBar->maximum();
    int currentBottom = scrollBar->value();
    return (totalHeight - currentBottom) <= 250;
}

bool ChatArea::isScrollAtBottom() const
{
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    return scrollBar->value() >= scrollBar->maximum() - 5;
}

void ChatArea::onNewMessageNotifierClicked()
{
    scrollToBottom();
}

void ChatArea::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateNewMessageNotifierPosition();
    updateInputBarPosition();

    // 确保新消息提示器在最上层
    if (newMessageNotifier && newMessageNotifier->isVisible()) {
        newMessageNotifier->raise();
    }
}

void ChatArea::updateNewMessageNotifier()
{
    if (unreadMessageCount > 0) {
        newMessageNotifier->setMessageCount(unreadMessageCount);
        newMessageNotifier->show();
        newMessageNotifier->raise();
        updateNewMessageNotifierPosition();
    } else {
        newMessageNotifier->hide();
    }
}

void ChatArea::updateNewMessageNotifierPosition()
{
    if (newMessageNotifier->isVisible() && inputBar) {
        // 计算新消息提示器的位置
        int x = (width() - newMessageNotifier->width()) / 2;
        // 将提示器放在输入栏上方
        const int BUTTOM_MARGIN = 26;
        int y = height() - inputBar->height() - newMessageNotifier->height() - BUTTOM_MARGIN;
        newMessageNotifier->move(x, y);
        newMessageNotifier->raise();  // 确保显示在最上层
    }
}

void ChatArea::scrollToBottom()
{
    chatView->scrollToBottom();
    
    isAtBottom = true;
    unreadMessageCount = 0;
    updateNewMessageNotifier();
}

void ChatArea::adjustBottomSpace()
{
    // 使用固定的底部空白高度
    chatModel->setBottomSpaceHeight(BottomSpace::DEFAULT_HEIGHT);
}

void ChatArea::addTextMessage(QSharedPointer<TextMessage> message,
                              const QDateTime &timestamp)
{
    bool isFromMe = message->isFromMe();
    bool shouldScroll = isFromMe || isNearBottom();

    // 设置消息时间戳为当前时间
    message->setTimestamp(timestamp);
    // 添加消息
    chatModel->addMessage(message);

    // 根据情况处理滚动和未读计数
    if (!isFromMe && !shouldScroll) {
        unreadMessageCount++;
        updateNewMessageNotifier();
    }

    // 最后处理滚动，确保新消息完全可见
    if (shouldScroll) {
        QTimer::singleShot(0, this, &ChatArea::scrollToBottom);
    }
}

void ChatArea::setGroupMode(bool mode) {
    isGroupMode = mode;
}

void ChatArea::updateInputBarPosition() {
    if (inputBar) {
        // 设置输入栏的大小和位置
        const int MARGIN = 20;  // 距离边缘的边距
        const int INPUT_BAR_WIDTH = width() - 2 * MARGIN;
        const int INPUT_BAR_HEIGHT = 175;  // 输入栏高度

        inputBar->setGeometry(
                MARGIN,  // x
                height() - INPUT_BAR_HEIGHT - MARGIN,  // y
                INPUT_BAR_WIDTH,  // width
                INPUT_BAR_HEIGHT  // height
        );
    }
}

void ChatArea::onSendImage(const QString &path)
{
    QPixmap image(path);
    if (!image.isNull()) {
        auto ptr =
                QSharedPointer<ImageMessage>::create(image,
                                               true,
                                               CurrentUser::instance().getUserId(),
                                               isGroupMode,
                                               CurrentUser::instance().getUserName(),
                                               GroupRole::Admin);
        addMessage(ptr);
    }
}

void ChatArea::onSendText(const QString &text)
{
    if (!text.trimmed().isEmpty()) {
        auto ptr =
                QSharedPointer<TextMessage>::create(text,
                                               true,
                                               CurrentUser::instance().getUserId(),
                                               isGroupMode,
                                               CurrentUser::instance().getUserName(),
                                               GroupRole::Admin);
        addMessage(ptr);
    }
}

void ChatArea::clearAll() {
    chatModel->clear();
}

void ChatArea::setMessageId(QString id) {
    messageId = id;
    if (isGroupMode) {
        auto group = GroupRepository::instance().getGroup(id);
        statusIcon->hide();
        QString fullName("%1（%2）");
        nameLabel->setText(fullName.arg(group.groupName, QString::number(group.memberNum)));
    }
    else {
        auto user = UserRepository::instance().getUser(id);
        statusIcon->show();
        QPixmap pixmap(QPixmap(statusIconPath(user.status)));
        statusIcon->setPixmap(pixmap.scaled(12, 12, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        nameLabel->setText(user.nick);
    }
}

void ChatArea::initMessage(QVector<ChatArea::ChatMessagePtr>& messages) {
    clearAll();
    for (auto message : messages) {
        chatModel->addMessage(message);
    }
    adjustBottomSpace();
}

