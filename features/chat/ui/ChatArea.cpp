#include "ChatArea.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "app/state/CurrentUser.h"
#include "shared/services/ImageService.h"
#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif
#include <QPainter>
#include <QLinearGradient>
#include <QVBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QResizeEvent>
#include <QDateTime>

namespace {

// Floating input bar metrics. Keep list bottom spacing in sync with these values.
static constexpr int kInputBarSideMargin = 20;
static constexpr int kInputBarBottomMargin = 18;
static constexpr int kInputBarHeight = 195;
static constexpr int kChatListBottomSpacePadding = 10;
static constexpr int kNewMessageNotifierBottomMargin = 26;
static constexpr int kInputBarBottomGradientFadeHeight = 32;
static constexpr int kInputBarBottomGradientSolidAlpha = 192;

class BottomGapGradientOverlay : public QWidget
{
public:
    explicit BottomGapGradientOverlay(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_TranslucentBackground);
        hide();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
        const qreal fadeStop = rect().height() > 0
                ? qBound(0.0,
                         static_cast<qreal>(kInputBarBottomGradientFadeHeight) / rect().height(),
                         1.0)
                : 1.0;
        gradient.setColorAt(0.0, QColor(0xF2, 0xF2, 0xF2, 0));
        gradient.setColorAt(fadeStop, QColor(0xF2, 0xF2, 0xF2, kInputBarBottomGradientSolidAlpha));
        gradient.setColorAt(1.0, QColor(0xF2, 0xF2, 0xF2, kInputBarBottomGradientSolidAlpha));
        painter.fillRect(rect(), gradient);
    }
};

} // namespace

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

    bottomGapGradientOverlay = new BottomGapGradientOverlay(this);

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
    connect(inputBar, &FloatingInputBar::sendTextAsPeer,
            this, &ChatArea::onSendTextAsPeer);
    connect(inputBar, &FloatingInputBar::inputFocused,
            this, &ChatArea::clearMessageSelection);

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
    emit sendMessage(messageId, previewTextForMessage(message.get()), message->getTimestamp());

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
    const int threshold = qMax(chatView->viewport()->height(), 180);
    return (scrollBar->maximum() - scrollBar->value()) <= threshold;
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
    adjustBottomSpace();

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
        int y = height() - inputBar->height() - newMessageNotifier->height() - kNewMessageNotifierBottomMargin;
        newMessageNotifier->move(x, y);
        newMessageNotifier->raise();  // 确保显示在最上层
    }
}

void ChatArea::scrollToBottom()
{
    chatView->scrollToBottom();
    unreadMessageCount = 0;
    updateNewMessageNotifier();
}

void ChatArea::adjustBottomSpace()
{
    if (!chatModel || !chatView || !inputBar) {
        return;
    }

    const QRect chatViewRect = chatView->geometry();
    const QRect inputBarRect = inputBar->geometry();
    const int inputBarOverlapHeight = qMax(0, chatViewRect.bottom() - inputBarRect.top() + 1);
    const int safeBottomSpace = inputBarOverlapHeight + kChatListBottomSpacePadding;

    chatModel->setBottomSpaceHeight(safeBottomSpace);
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

void ChatArea::setConversationMeta(const ConversationMeta& meta) {
    m_conversationMeta = meta;
    isGroupMode = meta.isGroup;
    if (meta.isGroup) {
        statusIcon->hide();
        nameLabel->setText(QString("%1（%2）").arg(meta.title, QString::number(meta.memberCount)));
        return;
    }

    statusIcon->show();
    statusIcon->setPixmap(ImageService::instance().scaled(statusIconPath(meta.status),
                                                          QSize(12, 12)));
    nameLabel->setText(meta.title);
}

void ChatArea::updateInputBarPosition() {
    if (inputBar) {
        const QRect inputBarRect(
                kInputBarSideMargin,
                height() - kInputBarHeight - kInputBarBottomMargin,
                width() - 2 * kInputBarSideMargin,
                kInputBarHeight);
        inputBar->setGeometry(inputBarRect);
        inputBar->raise();

#ifdef Q_OS_MACOS
        const bool useBottomGradient =
                MacFloatingInputBarBridge::appearance() == MacFloatingInputBarBridge::Appearance::LiquidGlass;
#else
        const bool useBottomGradient = false;
#endif
        if (bottomGapGradientOverlay) {
            const int gradientTop = inputBarRect.bottom() + 1;
            const int gradientHeight = height() - gradientTop;
            if (useBottomGradient && gradientHeight > 0) {
                const int overlayTop = qMax(0, gradientTop - kInputBarBottomGradientFadeHeight);
                bottomGapGradientOverlay->setGeometry(
                        0,
                        overlayTop,
                        width(),
                        height() - overlayTop);
                bottomGapGradientOverlay->show();
                bottomGapGradientOverlay->raise();
                inputBar->raise();
            } else {
                bottomGapGradientOverlay->hide();
            }
        }
    }
}

void ChatArea::onSendImage(const QString &path)
{
    if (ImageService::instance().sourceSize(path).isValid()) {
        auto ptr =
                QSharedPointer<ImageMessage>::create(path,
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

void ChatArea::onSendTextAsPeer(const QString& text)
{
    const QString trimmedText = text.trimmed();
    if (trimmedText.isEmpty()) {
        return;
    }

    QString senderId = m_conversationMeta.conversationId;
    QString senderName = m_conversationMeta.title;
    GroupRole role = GroupRole::Member;

    if (isGroupMode) {
        const Group group = GroupRepository::instance().requestGroupDetail({messageId});
        senderId = group.ownerId;
        senderName = UserRepository::instance().requestUserName(group.ownerId);
        role = GroupRole::Owner;
    }

    auto ptr = QSharedPointer<TextMessage>::create(trimmedText,
                                                   false,
                                                   senderId,
                                                   isGroupMode,
                                                   senderName,
                                                   role);
    addMessage(ptr);
}

void ChatArea::clearAll() {
    chatModel->clear();
    chatModel->clearSelection();
    unreadMessageCount = 0;
    isAtBottom = true;
    newMessageNotifier->hide();
}

void ChatArea::setMessageId(QString id) {
    messageId = id;
}

void ChatArea::initMessage(QVector<ChatArea::ChatMessagePtr>& messages) {
    clearAll();
    for (auto message : messages) {
        chatModel->addMessage(message);
    }
    adjustBottomSpace();
    QTimer::singleShot(0, chatView, &ChatListView::jumpToBottom);
    QTimer::singleShot(0, inputBar, &FloatingInputBar::focusInput);
}

QString ChatArea::previewTextForMessage(const ChatMessage* message) const
{
    if (!message) {
        return {};
    }

    if (!message->isInGroupChat()) {
        return message->getContent();
    }

    QString senderName = message->getSenderName();
    if (senderName.isEmpty()) {
        senderName = UserRepository::instance().requestUserName(message->getSenderId());
    }
    return QString("%1：%2").arg(senderName, message->getContent());
}

void ChatArea::clearMessageSelection()
{
    if (chatModel) {
        chatModel->clearSelection();
    }
}

void ChatArea::handleGlobalMousePress(const QPoint& globalPos)
{
    if (!chatModel || !chatView) {
        return;
    }

    if (inputBar && inputBar->rect().contains(inputBar->mapFromGlobal(globalPos))) {
        clearMessageSelection();
        return;
    }

    QWidget* viewport = chatView->viewport();
    if (!viewport || !viewport->rect().contains(viewport->mapFromGlobal(globalPos))) {
        clearMessageSelection();
        return;
    }

    const QPoint viewportPos = viewport->mapFromGlobal(globalPos);
    const QModelIndex index = chatView->indexAt(viewportPos);
    if (!index.isValid()) {
        clearMessageSelection();
        return;
    }

    if (chatModel->isBottomSpace(index.row()) || chatModel->isTimeHeader(index.row())) {
        clearMessageSelection();
        return;
    }

    QStyleOptionViewItem option;
    option.initFrom(chatView->viewport());
    option.rect = chatView->visualRect(index);
    if (!chatDelegate || !chatDelegate->bubbleHitTest(option, index, viewportPos)) {
        clearMessageSelection();
    }
}
