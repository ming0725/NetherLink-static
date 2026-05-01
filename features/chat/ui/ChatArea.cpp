#include "ChatArea.h"
#include "features/chat/data/GroupRepository.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "features/chat/ui/ConversationInfoPanel.h"
#include "features/chat/ui/ChatSessionController.h"
#include "app/state/CurrentUser.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#ifdef Q_OS_MACOS
#include "platform/macos/MacFloatingInputBarBridge_p.h"
#endif
#include <QPainter>
#include <QLinearGradient>
#include <QHBoxLayout>
#include <QScrollBar>
#include <QTimer>
#include <QResizeEvent>
#include <QPaintEvent>
#include <QDateTime>
#include <QFont>
#include <QPropertyAnimation>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>

namespace {

// Floating input bar metrics. Keep list bottom spacing in sync with these values.
static constexpr int kChatInfoHeight = 62;
static constexpr int kInputBarSideMargin = 20;
static constexpr int kInputBarBottomMargin = 18;
static constexpr int kInputBarHeight = 195;
static constexpr int kChatListBottomSpacePadding = 10;
static constexpr int kNewMessageNotifierBottomMargin = 26;
static constexpr int kInputBarBottomGradientFadeHeight = 32;
static constexpr int kInputBarBottomGradientSolidAlpha = 192;
static constexpr int kOlderMessagePageSize = 24;
static constexpr int kFetchOlderTopThreshold = 8;
static constexpr int kInfoButtonSize = 28;
static constexpr int kInfoPanelDividerHeight = 1;
static constexpr int kInfoPanelMinWidth = 220;
static constexpr int kInfoPanelPreferredWidth = 280;
static constexpr int kInfoPanelMaxWidth = 340;
static constexpr int kInfoPanelAnimationDuration = 220;
static constexpr int kChatInfoRightMargin = 18;

class SquareDotsButton : public QPushButton
{
public:
    explicit SquareDotsButton(QWidget* parent = nullptr)
        : QPushButton(parent)
    {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setFlat(true);
        setFixedSize(kInfoButtonSize, kInfoButtonSize);
        setAttribute(Qt::WA_Hover);
    }

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        if (isDown()) {
            painter.setBrush(ThemeManager::instance().color(ThemeColor::Divider));
        } else if (underMouse()) {
            painter.setBrush(ThemeManager::instance().color(ThemeColor::ListHover));
        } else {
            painter.setBrush(Qt::transparent);
        }
        painter.setPen(Qt::NoPen);
        painter.drawRoundedRect(rect().adjusted(1, 1, -1, -1), 6, 6);

        painter.setBrush(ThemeManager::instance().color(ThemeColor::SecondaryText));
        const int dotSize = 4;
        const int spacing = 4;
        const int totalWidth = dotSize * 3 + spacing * 2;
        const int startX = (width() - totalWidth) / 2;
        const int y = (height() - dotSize) / 2;
        for (int i = 0; i < 3; ++i) {
            painter.drawRect(startX + i * (dotSize + spacing), y, dotSize, dotSize);
        }
    }
};

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
        QColor pageBackground = ThemeManager::instance().color(ThemeColor::PageBackground);
        pageBackground.setAlpha(0);
        gradient.setColorAt(0.0, pageBackground);
        pageBackground.setAlpha(kInputBarBottomGradientSolidAlpha);
        gradient.setColorAt(fadeStop, pageBackground);
        gradient.setColorAt(1.0, pageBackground);
        painter.fillRect(rect(), gradient);
    }
};

class ThemeFillWidget : public QWidget
{
public:
    explicit ThemeFillWidget(ThemeColor role, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_role(role)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), ThemeManager::instance().color(m_role));
    }

private:
    ThemeColor m_role;
};

class ThemeTextLabel : public QLabel
{
public:
    explicit ThemeTextLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);
        QPainter painter(this);
        painter.setFont(font());
        painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        painter.drawText(rect(), alignment() | Qt::TextSingleLine, text());
    }
};

} // namespace

ChatArea::ChatArea(QWidget *parent)
        : QWidget(parent)
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
    sessionController = new ChatSessionController(this);

    infoPanelAnimation = new QPropertyAnimation(this);
    infoPanelAnimation->setPropertyName("geometry");
    infoPanelAnimation->setDuration(kInfoPanelAnimationDuration);
    infoPanelAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(infoPanelAnimation, &QPropertyAnimation::finished, this, [this]() {
        if (!infoPanelOpen) {
            releaseInfoPanels();
        }
        updateInputBarPosition();
        adjustBottomSpace();
    });
    connect(infoPanelAnimation, &QPropertyAnimation::valueChanged, this, [this]() {
        updateInputBarPosition();
        adjustBottomSpace();
        if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
            panel->raise();
        }
    });

    // 创建悬浮输入栏
    inputBar = new FloatingInputBar(this);
    inputBar->show();

    QWidget* chatInfo = new ThemeFillWidget(ThemeColor::PageBackground, this);
    chatInfo->setFixedHeight(kChatInfoHeight);
    QWidget* chatInfoDivider = new ThemeFillWidget(ThemeColor::Divider, this);
    chatInfoDivider->setFixedHeight(1);

    // 外层垂直布局：用于将内容推到底部
    QVBoxLayout* outerLayout = new QVBoxLayout(chatInfo);
    outerLayout->setContentsMargins(20, 5, kChatInfoRightMargin, 10); // 留出左右下边距
    outerLayout->addStretch(); // 将内容推到底部

    // 内层水平布局：图标 + 名字
    QHBoxLayout* bottomLayout = new QHBoxLayout();
    bottomLayout->setSpacing(5); // 图标与文字间距
    bottomLayout->setContentsMargins(0, 0, 0, 0);

    // 在线状态图标
    statusIcon = new QLabel(chatInfo);

    // 名字 Label
    nameLabel = new ThemeTextLabel(chatInfo);
    QFont nameFont = nameLabel->font();
    nameFont.setPixelSize(17);
    nameLabel->setFont(nameFont);
    auto applyHeaderTheme = [this, chatInfo, chatInfoDivider]() {
        nameLabel->update();
        chatInfo->update();
        chatInfoDivider->update();
        if (chatView) {
            chatView->viewport()->update();
        }
    };
    applyHeaderTheme();

    infoButton = new SquareDotsButton(chatInfo);

    // 添加到底部布局
    bottomLayout->addWidget(nameLabel);
    bottomLayout->addWidget(statusIcon);
    bottomLayout->addStretch(); // 保证名称靠左，入口靠右
    bottomLayout->addWidget(infoButton);

    // 添加到底部区域
    outerLayout->addLayout(bottomLayout);

    chatInfo->setLayout(outerLayout);
    
    // 添加到主布局
    mainLayout->addWidget(chatInfo);
    mainLayout->addWidget(chatInfoDivider);
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
    connect(chatDelegate, &ChatItemDelegate::deleteRequested,
            this, &ChatArea::onDeleteMessageRequested);
    connect(infoButton, &QPushButton::clicked,
            this, &ChatArea::onInfoButtonClicked);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, applyHeaderTheme);
    connect(sessionController, &ChatSessionController::sessionChanged,
            this, [this](const ConversationMeta& meta, const User&, const Group&) {
                onSessionChanged(meta);
            });
    connect(sessionController, &ChatSessionController::directPanelDataLoaded,
            this, &ChatArea::onDirectPanelDataLoaded);
    connect(sessionController, &ChatSessionController::groupPanelDataLoaded,
            this, &ChatArea::onGroupPanelDataLoaded);
    connect(sessionController, &ChatSessionController::groupMembersPageLoaded,
            this, &ChatArea::onGroupMembersPageLoaded);
    connect(sessionController, &ChatSessionController::messagesCleared,
            this, &ChatArea::onSessionMessagesCleared);
    connect(sessionController, &ChatSessionController::conversationRemoved,
            this, &ChatArea::onSessionConversationRemoved);

}


void ChatArea::addMessage(QSharedPointer<ChatMessage> message)
{
    if (!message || conversationId().isEmpty()) {
        return;
    }

    bool shouldScroll = message->isFromMe() || isNearBottom();
    // 添加消息
    chatModel->addMessage(message);
    MessageRepository::instance().addMessage(conversationId(), message);
    ++m_state.loadedMessageCount;

    // 调整底部空白
    adjustBottomSpace();
    
    // 根据情况处理滚动和未读计数
    if (!message->isFromMe() && !shouldScroll) {
        ++m_state.unreadMessageCount;
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
    if (!message) {
        return;
    }
    message->setTimestamp(timestamp);
    addMessage(std::move(message));
}

void ChatArea::onScrollValueChanged(int)
{
    m_state.isAtBottom = isScrollAtBottom();
    
    if (m_state.isAtBottom) {
        m_state.unreadMessageCount = 0;
        updateNewMessageNotifier();
    }

    if (m_state.allowOlderMessageFetch &&
        m_state.hasMoreBefore &&
        !m_state.loadingOlderMessages &&
        chatView->verticalScrollBar()->value() <= kFetchOlderTopThreshold) {
        loadOlderMessages();
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
    updateInfoPanelGeometry();
    updateNewMessageNotifierPosition();
    updateInputBarPosition();
    adjustBottomSpace();

    // 确保新消息提示器在最上层
    if (newMessageNotifier && newMessageNotifier->isVisible()) {
        newMessageNotifier->raise();
    }
    if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
        panel->raise();
    }
}

void ChatArea::updateNewMessageNotifier()
{
    if (m_state.unreadMessageCount > 0) {
        newMessageNotifier->setMessageCount(m_state.unreadMessageCount);
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
    m_state.unreadMessageCount = 0;
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
    if (!message) {
        return;
    }
    message->setTimestamp(timestamp);
    addMessage(std::move(message));
}

void ChatArea::applyConversationMeta()
{
    if (infoPanelOpen) {
        requestInfoPanelData(false);
    }
    if (m_state.meta.isGroup) {
        statusIcon->hide();
        nameLabel->setText(QString("%1（%2）").arg(m_state.meta.title, QString::number(m_state.meta.memberCount)));
        return;
    }

    statusIcon->show();
    statusIcon->setPixmap(ImageService::instance().scaled(statusIconPath(m_state.meta.status),
                                                          QSize(12, 12)));
    nameLabel->setText(m_state.meta.title);
}

void ChatArea::onSessionChanged(const ConversationMeta& meta)
{
    m_state.meta = meta;
    applyConversationMeta();
}

QWidget* ChatArea::activeInfoPanel() const
{
    return isGroupMode()
            ? static_cast<QWidget*>(groupInfoPanel)
            : static_cast<QWidget*>(directInfoPanel);
}

QWidget* ChatArea::inactiveInfoPanel() const
{
    return isGroupMode()
            ? static_cast<QWidget*>(directInfoPanel)
            : static_cast<QWidget*>(groupInfoPanel);
}

QWidget* ChatArea::ensureActiveInfoPanel()
{
    if (isGroupMode()) {
        if (!groupInfoPanel) {
            groupInfoPanel = new GroupConversationInfoPanel(this);
            groupInfoPanel->setObjectName("groupConversationInfoPanel");
            groupInfoPanel->setProperty("conversationType", "group");
            connectGroupInfoPanel(groupInfoPanel);
        }
        return groupInfoPanel;
    }

    if (!directInfoPanel) {
        directInfoPanel = new DirectConversationInfoPanel(this);
        directInfoPanel->setObjectName("directConversationInfoPanel");
        directInfoPanel->setProperty("conversationType", "direct");
        connectDirectInfoPanel(directInfoPanel);
    }
    return directInfoPanel;
}

void ChatArea::connectGroupInfoPanel(GroupConversationInfoPanel* panel)
{
    if (!panel || !sessionController) {
        return;
    }

    connect(panel, &GroupConversationInfoPanel::groupNameChanged,
            sessionController, &ChatSessionController::saveGroupName);
    connect(panel, &GroupConversationInfoPanel::groupIntroductionChanged,
            sessionController, &ChatSessionController::saveGroupIntroduction);
    connect(panel, &GroupConversationInfoPanel::groupAnnouncementChanged,
            sessionController, &ChatSessionController::saveGroupAnnouncement);
    connect(panel, &GroupConversationInfoPanel::currentUserNicknameChanged,
            sessionController, &ChatSessionController::saveCurrentUserGroupNickname);
    connect(panel, &GroupConversationInfoPanel::groupMemberNicknameChanged,
            sessionController, &ChatSessionController::saveGroupMemberNickname);
    connect(panel, &GroupConversationInfoPanel::groupMemberAdminPromotionRequested,
            sessionController, &ChatSessionController::promoteGroupMemberToAdmin);
    connect(panel, &GroupConversationInfoPanel::groupMemberAdminCancellationRequested,
            sessionController, &ChatSessionController::cancelGroupMemberAdmin);
    connect(panel, &GroupConversationInfoPanel::groupMemberRemovalRequested,
            sessionController, &ChatSessionController::removeGroupMember);
    connect(panel, &GroupConversationInfoPanel::groupRemarkChanged,
            sessionController, &ChatSessionController::saveGroupRemark);
    connect(panel, &GroupConversationInfoPanel::pinChanged,
            sessionController, &ChatSessionController::setPinned);
    connect(panel, &GroupConversationInfoPanel::doNotDisturbChanged,
            sessionController, &ChatSessionController::setDoNotDisturb);
    connect(panel, &GroupConversationInfoPanel::clearChatHistoryRequested,
            this, &ChatArea::confirmClearChatHistory);
    connect(panel, &GroupConversationInfoPanel::exitGroupRequested,
            this, &ChatArea::confirmExitGroup);
    connect(panel, &GroupConversationInfoPanel::groupMembersPageRequested,
            sessionController, &ChatSessionController::loadGroupMembersPage);
}

void ChatArea::connectDirectInfoPanel(DirectConversationInfoPanel* panel)
{
    if (!panel || !sessionController) {
        return;
    }

    connect(panel, &DirectConversationInfoPanel::remarkChanged,
            sessionController, &ChatSessionController::saveDirectRemark);
    connect(panel, &DirectConversationInfoPanel::pinChanged,
            sessionController, &ChatSessionController::setPinned);
    connect(panel, &DirectConversationInfoPanel::doNotDisturbChanged,
            sessionController, &ChatSessionController::setDoNotDisturb);
    connect(panel, &DirectConversationInfoPanel::clearChatHistoryRequested,
            this, &ChatArea::confirmClearChatHistory);
    connect(panel, &DirectConversationInfoPanel::deleteFriendRequested,
            this, &ChatArea::confirmDeleteFriend);
}

void ChatArea::releaseInfoPanels()
{
    if (sessionController) {
        sessionController->cancelPanelLoads();
    }
    if (infoPanelAnimation) {
        infoPanelAnimation->setTargetObject(nullptr);
    }
    if (groupInfoPanel) {
        groupInfoPanel->releaseTransientResources();
        groupInfoPanel->deleteLater();
        groupInfoPanel = nullptr;
    }
    if (directInfoPanel) {
        directInfoPanel->deleteLater();
        directInfoPanel = nullptr;
    }
}

void ChatArea::requestInfoPanelData(bool resetTransientState)
{
    if (!sessionController || conversationId().isEmpty()) {
        return;
    }

    if (resetTransientState && isGroupMode()) {
        if (groupInfoPanel) {
            groupInfoPanel->resetTransientState();
            groupInfoPanel->setGroupSummary(Group{},
                                            m_state.meta,
                                            {},
                                            m_state.meta.memberCount,
                                            false,
                                            false);
        }
    } else if (resetTransientState && directInfoPanel) {
        directInfoPanel->setConversationMeta(m_state.meta, QString(), false);
    }
    sessionController->loadPanelData();
}

QRect ChatArea::infoPanelOpenGeometry() const
{
    const int maxPanelWidth = qMax(0, width() - 48);
    const int boundedWidth = qBound(kInfoPanelMinWidth,
                                    qMin(kInfoPanelPreferredWidth, width() * 2 / 5),
                                    kInfoPanelMaxWidth);
    const int panelWidth = qMin(boundedWidth, maxPanelWidth);
    const int panelTop = kChatInfoHeight + kInfoPanelDividerHeight;
    return QRect(width() - panelWidth,
                 panelTop,
                 panelWidth,
                 qMax(0, height() - panelTop));
}

QRect ChatArea::infoPanelClosedGeometry() const
{
    QRect openRect = infoPanelOpenGeometry();
    openRect.moveLeft(width());
    return openRect;
}

int ChatArea::visibleInfoPanelWidth() const
{
    QWidget* panel = activeInfoPanel();
    if (!panel || !panel->isVisible()) {
        return 0;
    }

    return qBound(0, width() - panel->geometry().left(), infoPanelOpenGeometry().width());
}

void ChatArea::showInfoPanel(bool animated)
{
    QWidget* panel = ensureActiveInfoPanel();
    QWidget* hiddenPanel = inactiveInfoPanel();
    if (!panel) {
        return;
    }

    requestInfoPanelData(true);

    if (infoPanelAnimation) {
        infoPanelAnimation->stop();
    }
    if (hiddenPanel) {
        hiddenPanel->hide();
    }

    infoPanelOpen = true;
    const QRect openRect = infoPanelOpenGeometry();
    panel->setGeometry(animated ? infoPanelClosedGeometry() : openRect);
    panel->show();
    panel->raise();
    if (infoButton) {
        infoButton->raise();
    }

    if (!animated || !infoPanelAnimation) {
        panel->setGeometry(openRect);
        return;
    }

    infoPanelAnimation->setTargetObject(panel);
    infoPanelAnimation->setStartValue(panel->geometry());
    infoPanelAnimation->setEndValue(openRect);
    infoPanelAnimation->start();
}

void ChatArea::hideInfoPanel(bool animated)
{
    QWidget* panel = activeInfoPanel();
    if (!panel || (!panel->isVisible() && !infoPanelOpen)) {
        infoPanelOpen = false;
        releaseInfoPanels();
        return;
    }

    if (infoPanelAnimation) {
        infoPanelAnimation->stop();
    }

    infoPanelOpen = false;
    const QRect closedRect = infoPanelClosedGeometry();

    if (!animated || !infoPanelAnimation) {
        releaseInfoPanels();
        return;
    }

    panel->show();
    panel->raise();
    infoPanelAnimation->setTargetObject(panel);
    infoPanelAnimation->setStartValue(panel->geometry());
    infoPanelAnimation->setEndValue(closedRect);
    infoPanelAnimation->start();
}

void ChatArea::updateInfoPanelGeometry()
{
    if (infoPanelAnimation && infoPanelAnimation->state() == QPropertyAnimation::Running) {
        infoPanelAnimation->stop();
    }

    const QRect targetRect = infoPanelOpen ? infoPanelOpenGeometry() : infoPanelClosedGeometry();
    if (groupInfoPanel) {
        groupInfoPanel->setGeometry(targetRect);
        groupInfoPanel->setVisible(infoPanelOpen && isGroupMode());
    }
    if (directInfoPanel) {
        directInfoPanel->setGeometry(targetRect);
        directInfoPanel->setVisible(infoPanelOpen && !isGroupMode());
    }

    if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
        panel->raise();
    }
    if (infoButton) {
        infoButton->raise();
    }
}

bool ChatArea::containsGlobalPoint(QWidget* widget, const QPoint& globalPos) const
{
    return widget && widget->isVisible() && widget->rect().contains(widget->mapFromGlobal(globalPos));
}

void ChatArea::updateGroupInfoPanelState()
{
    if (!groupInfoPanel || !infoPanelOpen) {
        return;
    }

    requestInfoPanelData(false);
}

void ChatArea::updateDirectInfoPanelState(bool animated)
{
    if (directInfoPanel && infoPanelOpen) {
        const QString remark = sessionController ? sessionController->directUser().remark : QString{};
        directInfoPanel->setConversationMeta(m_state.meta, remark, animated);
    }
}

void ChatArea::onDirectPanelDataLoaded(const ConversationMeta& meta, const User& directUser)
{
    if (!directInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || meta.isGroup) {
        return;
    }

    m_state.meta = meta;
    directInfoPanel->setConversationMeta(meta, directUser.remark, true);
}

void ChatArea::onGroupPanelDataLoaded(const ConversationMeta& meta,
                                      const Group& group,
                                      const QVector<User>& previewMembers,
                                      int totalMembers,
                                      bool canEditGroupInfo,
                                      bool canExitGroup)
{
    if (!groupInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || !meta.isGroup) {
        return;
    }

    m_state.meta = meta;
    groupInfoPanel->setGroupSummary(group,
                                    meta,
                                    previewMembers,
                                    totalMembers,
                                    canEditGroupInfo,
                                    canExitGroup);
}

void ChatArea::onGroupMembersPageLoaded(const ConversationMeta& meta, const GroupMembersPage& page)
{
    if (!groupInfoPanel || !infoPanelOpen || meta.conversationId != conversationId() || !meta.isGroup) {
        return;
    }

    groupInfoPanel->appendGroupMembersPage(page);
}

void ChatArea::onInfoButtonClicked()
{
    if (infoPanelOpen) {
        hideInfoPanel(true);
    } else {
        showInfoPanel(true);
    }
}

void ChatArea::confirmClearChatHistory()
{
    if (conversationId().isEmpty()) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("删除聊天记录"),
                                             QStringLiteral("确认删除当前聊天记录吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    if (sessionController) {
        sessionController->clearMessages();
    }
}

void ChatArea::onSessionMessagesCleared()
{
    chatModel->clear();
    m_state.loadedMessageCount = 0;
    m_state.unreadMessageCount = 0;
    m_state.hasMoreBefore = false;
    m_state.loadingOlderMessages = false;
    newMessageNotifier->hide();
    adjustBottomSpace();
}

void ChatArea::confirmExitGroup()
{
    if (conversationId().isEmpty() || !isGroupMode()) {
        return;
    }

    if (!sessionController || !sessionController->canExitGroup()) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("退出群聊"),
                                             QStringLiteral("确认退出该群聊吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    sessionController->exitGroup();
}

void ChatArea::confirmDeleteFriend()
{
    if (conversationId().isEmpty() || isGroupMode()) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("删除好友"),
                                             QStringLiteral("确认删除该好友吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    if (sessionController) {
        sessionController->deleteFriend();
    }
}

void ChatArea::onSessionConversationRemoved()
{
    clearConversation();
    emit currentConversationRemoved();
}

void ChatArea::updateInputBarPosition() {
    if (inputBar) {
        const int infoPanelWidth = visibleInfoPanelWidth();
        const QRect inputBarRect(
                kInputBarSideMargin,
                height() - kInputBarHeight - kInputBarBottomMargin,
                qMax(0, width() - 2 * kInputBarSideMargin - infoPanelWidth),
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
        if (QWidget* panel = activeInfoPanel(); panel && panel->isVisible()) {
            panel->raise();
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
                                               isGroupMode(),
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
                                               isGroupMode(),
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

    QString senderId = m_state.meta.conversationId;
    QString senderName = m_state.meta.title;
    GroupRole role = GroupRole::Member;

    if (isGroupMode()) {
        const Group group = GroupRepository::instance().requestGroupDetail({conversationId()});
        senderId = group.ownerId;
        senderName = UserRepository::instance().requestUserName(group.ownerId);
        role = GroupRole::Owner;
    }

    auto ptr = QSharedPointer<TextMessage>::create(trimmedText,
                                                   false,
                                                   senderId,
                                                   isGroupMode(),
                                                   senderName,
                                                   role);
    addMessage(ptr);
}

void ChatArea::onDeleteMessageRequested(int row)
{
    if (conversationId().isEmpty() || !chatModel) {
        return;
    }

    const QSharedPointer<ChatMessage> message = chatModel->sharedMessageAt(row);
    if (message.isNull()) {
        return;
    }

    if (!MessageRepository::instance().removeMessage(conversationId(), message)) {
        return;
    }

    if (chatModel->removeMessage(row) && m_state.loadedMessageCount > 0) {
        --m_state.loadedMessageCount;
    }
    adjustBottomSpace();
}

void ChatArea::clearConversation(bool closeInfoPanel)
{
    if (closeInfoPanel) {
        hideInfoPanel(false);
    }
    if (sessionController) {
        sessionController->close();
    }
    chatModel->clear();
    chatModel->clearSelection();
    m_state = {};
    nameLabel->clear();
    statusIcon->hide();
    newMessageNotifier->hide();
    updateGroupInfoPanelState();
    updateDirectInfoPanelState(false);
}

void ChatArea::closeConversation()
{
    clearConversation();
}

void ChatArea::loadOlderMessages()
{
    if (conversationId().isEmpty() || !m_state.hasMoreBefore || m_state.loadingOlderMessages) {
        return;
    }

    m_state.loadingOlderMessages = true;
    QScrollBar* scrollBar = chatView->verticalScrollBar();
    const int previousValue = scrollBar->value();
    const int previousMaximum = scrollBar->maximum();

    const ConversationThreadData olderPage = MessageRepository::instance().requestConversationThread({
            conversationId(),
            m_state.loadedMessageCount,
            kOlderMessagePageSize
    });

    if (olderPage.messages.isEmpty()) {
        m_state.hasMoreBefore = false;
        m_state.loadingOlderMessages = false;
        return;
    }

    chatModel->prependMessages(olderPage.messages);
    m_state.loadedMessageCount = olderPage.loadedMessageCount;
    m_state.hasMoreBefore = olderPage.hasMoreBefore;
    adjustBottomSpace();
    chatView->preserveScrollPositionAfterPrepend(previousValue, previousMaximum);
    m_state.loadingOlderMessages = false;
}

QString ChatArea::conversationId() const
{
    return m_state.meta.conversationId;
}

bool ChatArea::isGroupMode() const
{
    return m_state.meta.isGroup;
}

void ChatArea::openConversation(const ConversationThreadData& conversation)
{
    if (infoPanelOpen) {
        hideInfoPanel(true);
    }
    clearConversation(false);
    if (sessionController) {
        sessionController->open(conversation.meta);
    } else {
        m_state.meta = conversation.meta;
    }
    m_state.loadedMessageCount = conversation.loadedMessageCount;
    m_state.hasMoreBefore = conversation.hasMoreBefore;
    m_state.allowOlderMessageFetch = false;
    chatModel->setMessages(conversation.messages);
    adjustBottomSpace();
    QTimer::singleShot(0, this, [this]() {
        chatView->jumpToBottom();
        m_state.allowOlderMessageFetch = true;
    });
    QTimer::singleShot(0, inputBar, &FloatingInputBar::focusInput);
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

    if (containsGlobalPoint(infoButton, globalPos)) {
        return;
    }

    if (infoPanelOpen) {
        QWidget* panel = activeInfoPanel();
        if (containsGlobalPoint(panel, globalPos)) {
            clearMessageSelection();
            return;
        }
        hideInfoPanel(true);
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
