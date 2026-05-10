#include "AiChatConversationWidget.h"

#include <QLabel>
#include <QLinearGradient>
#include <QPainter>
#include <QRandomGenerator>
#include <QResizeEvent>
#include <QStringList>
#include <QTimer>

#include "features/aichat/data/AiChatRepository.h"
#include "features/aichat/model/AiChatMessageListModel.h"
#include "features/aichat/ui/AiChatFloatingInputBar.h"
#include "features/aichat/ui/AiChatMessageListView.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kBottomGradientFadeHeight = 32;
constexpr int kBottomGradientSolidAlpha = 192;

class ThemeDivider : public QWidget
{
public:
    explicit ThemeDivider(QWidget* parent = nullptr)
        : QWidget(parent)
    {
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QPainter painter(this);
        painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::Divider));
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
        painter.setRenderHint(QPainter::Antialiasing, true);
        painter.setRenderHint(QPainter::TextAntialiasing, true);

        QLinearGradient gradient(rect().topLeft(), rect().bottomLeft());
        const qreal fadeStop = rect().height() > 0
                ? qBound(0.0,
                         static_cast<qreal>(kBottomGradientFadeHeight) / rect().height(),
                         1.0)
                : 1.0;
        QColor pageBackground = ThemeManager::instance().color(ThemeColor::PageBackground);
        pageBackground.setAlpha(0);
        gradient.setColorAt(0.0, pageBackground);
        pageBackground.setAlpha(kBottomGradientSolidAlpha);
        gradient.setColorAt(fadeStop, pageBackground);
        gradient.setColorAt(1.0, pageBackground);
        painter.fillRect(rect(), gradient);
    }
};

} // namespace

AiChatConversationWidget::AiChatConversationWidget(QWidget* parent)
    : QWidget(parent)
    , m_messageView(new AiChatMessageListView(this))
    , m_messageModel(new AiChatMessageListModel(this))
    , m_inputBar(new AiChatFloatingInputBar(this))
    , m_titleLabel(new QLabel(this))
    , m_headerDivider(new ThemeDivider(this))
    , m_bottomGapGradientOverlay(new BottomGapGradientOverlay(this))
    , m_emptyLabel(new QLabel(QStringLiteral("选择或新建一个 AI 对话"), this))
    , m_streamTimer(new QTimer(this))
{
    m_messageView->setModel(m_messageModel);
    m_streamTimer->setInterval(24);
    m_titleLabel->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_emptyLabel->setAlignment(Qt::AlignCenter);
    m_emptyLabel->setAttribute(Qt::WA_TransparentForMouseEvents);

    QFont titleFont = m_titleLabel->font();
    titleFont.setPixelSize(17);
    m_titleLabel->setFont(titleFont);

    QFont emptyFont = m_emptyLabel->font();
    emptyFont.setPixelSize(15);
    m_emptyLabel->setFont(emptyFont);

    connect(m_inputBar, &AiChatFloatingInputBar::sendText,
            this, &AiChatConversationWidget::onSendText);
    connect(m_inputBar, &AiChatFloatingInputBar::inputFocused,
            m_messageView, &AiChatMessageListView::clearTextSelection);
    connect(m_streamTimer, &QTimer::timeout,
            this, &AiChatConversationWidget::advanceAiReplyStream);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateHeader();
        update();
        m_headerDivider->update();
        m_bottomGapGradientOverlay->update();
        m_messageView->viewport()->update();
    });

    closeConversation();
}

void AiChatConversationWidget::openConversation(const AiChatListEntry& entry)
{
    if (entry.conversationId.isEmpty()) {
        closeConversation();
        return;
    }

    finishActiveAiReplyStream();
    m_currentConversation = entry;
    m_messageModel->setMessages(AiChatRepository::instance().requestAiChatMessages(entry.conversationId));
    m_messageView->clearTextSelection();
    m_messageView->show();
    m_inputBar->show();
    m_emptyLabel->hide();
    updateHeader();
    updateLayout();
    m_messageView->scrollToBottom();
    QTimer::singleShot(0, m_inputBar, [this]() {
        if (m_inputBar->isVisible() && !m_currentConversation.conversationId.isEmpty()) {
            m_inputBar->focusInput();
        }
    });
}

void AiChatConversationWidget::closeConversation()
{
    finishActiveAiReplyStream();
    m_currentConversation = {};
    m_messageModel->clear();
    m_messageView->clearTextSelection();
    m_messageView->hide();
    m_inputBar->hide();
    m_emptyLabel->show();
    updateHeader();
    updateLayout();
}

void AiChatConversationWidget::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PageBackground));
}

void AiChatConversationWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateLayout();
}

void AiChatConversationWidget::onSendText(const QString& text)
{
    if (m_currentConversation.conversationId.isEmpty()) {
        return;
    }

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            m_currentConversation.conversationId,
            text,
            true);
    if (message.messageId.isEmpty()) {
        return;
    }

    m_messageModel->appendMessage(message);
    m_messageView->clearTextSelection();
    m_messageView->scrollToBottom();
    finishActiveAiReplyStream();
    appendAiReply(text);
}

void AiChatConversationWidget::updateLayout()
{
    m_titleLabel->setGeometry(20, 0, qMax(0, width() - 40), kHeaderHeight);
    m_headerDivider->setGeometry(0, kHeaderHeight, width(), 1);
    m_messageView->setGeometry(0, kHeaderHeight + 1, width(), qMax(0, height() - kHeaderHeight - 1));
    m_emptyLabel->setGeometry(0, kHeaderHeight + 1, width(), qMax(0, height() - kHeaderHeight - 1));

    const int inputWidth = qMax(0, width() - kInputBarSideMargin * 2);
    const int inputX = kInputBarSideMargin;
    const int inputY = height() - kInputBarBottomMargin - kInputBarHeight;
    m_inputBar->setGeometry(inputX,
                            qMax(kHeaderHeight + 1, inputY),
                            inputWidth,
                            kInputBarHeight);

    updateBottomSpace();
    m_inputBar->raise();
}

void AiChatConversationWidget::updateBottomSpace()
{
    if (!m_messageView || !m_inputBar || !m_bottomGapGradientOverlay) {
        return;
    }

    const QRect messageViewRect = m_messageView->geometry();
    const QRect inputBarRect = m_inputBar->geometry();
    const int inputBarOverlapHeight = qMax(0, messageViewRect.bottom() - inputBarRect.top() + 1);
    const int safeBottomSpace = inputBarOverlapHeight + kListBottomPadding;

    m_messageView->setBottomViewportMargin(safeBottomSpace);

    if (inputBarOverlapHeight > 0 && m_inputBar->isVisible()) {
        const int gradientTop = inputBarRect.top();
        const int overlayTop = qMax(messageViewRect.top(),
                                    gradientTop - kBottomGradientFadeHeight);
        const int overlayBottom = messageViewRect.bottom();
        m_bottomGapGradientOverlay->setGeometry(messageViewRect.left(),
                                                overlayTop,
                                                messageViewRect.width(),
                                                qMax(0, overlayBottom - overlayTop + 1));
        m_bottomGapGradientOverlay->show();
        m_bottomGapGradientOverlay->raise();
    } else {
        m_bottomGapGradientOverlay->hide();
    }

    m_inputBar->raise();
}

void AiChatConversationWidget::updateHeader()
{
    const bool hasConversation = !m_currentConversation.conversationId.isEmpty();
    m_titleLabel->setText(hasConversation ? m_currentConversation.title : QStringLiteral("AI 对话"));
    m_titleLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name()));
    m_emptyLabel->setStyleSheet(QStringLiteral("color: %1; background: transparent;")
                                .arg(ThemeManager::instance().color(ThemeColor::SecondaryText).name()));
}

void AiChatConversationWidget::appendAiReply(const QString& prompt)
{
    if (m_currentConversation.conversationId.isEmpty()) {
        return;
    }

    m_streamReplyText = randomReplyText(prompt);
    m_streamConversationId = m_currentConversation.conversationId;
    m_streamOffset = qMin(4, m_streamReplyText.size());

    const AiChatMessage message = AiChatRepository::instance().addAiChatMessage(
            m_streamConversationId,
            m_streamReplyText.left(m_streamOffset),
            false);
    if (message.messageId.isEmpty()) {
        m_streamConversationId.clear();
        m_streamReplyText.clear();
        m_streamOffset = 0;
        return;
    }

    m_streamMessageId = message.messageId;
    m_messageModel->appendMessage(message);
    m_messageView->scrollToBottom();
    m_streamTimer->start();
}

void AiChatConversationWidget::advanceAiReplyStream()
{
    if (m_streamConversationId.isEmpty() || m_streamMessageId.isEmpty() || m_streamReplyText.isEmpty()) {
        m_streamTimer->stop();
        return;
    }

    if (m_streamOffset >= m_streamReplyText.size()) {
        finishActiveAiReplyStream();
        return;
    }

    const int chunkSize = QRandomGenerator::global()->bounded(2, 7);
    m_streamOffset = qMin(m_streamOffset + chunkSize, m_streamReplyText.size());
    const QString visibleText = m_streamReplyText.left(m_streamOffset);
    AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                         m_streamMessageId,
                                                         visibleText);

    if (m_currentConversation.conversationId == m_streamConversationId) {
        m_messageModel->updateMessageText(m_streamMessageId, visibleText);
        m_messageView->scrollToBottom();
    }

    if (m_streamOffset >= m_streamReplyText.size()) {
        finishActiveAiReplyStream();
    }
}

QString AiChatConversationWidget::randomReplyText(const QString& prompt) const
{
    const QString promptPreview = prompt.simplified().left(80);
    const QStringList replies = {
        QStringLiteral("我先按本地演示模式回复，不接任何真实模型。你刚才输入的是：%1。\n\n为了方便测试滚动、换行、长文本气泡、字符命中、复制选择和底部悬浮输入栏遮挡关系，这条内容会刻意写得比较长。它会分成很多小段逐步出现，模拟真实 AI 回复的流式传输。你可以在回复还没结束的时候滚动列表、拖拽选中文本、复制其中一段，或者继续输入下一条消息，观察列表是否能保持在底部并且输入栏不被其它控件挡住。")
                .arg(promptPreview),
        QStringLiteral("这是一个随机命中的测试回复。当前不会调用任何智能能力，只会从几段固定文本里挑选一段，然后通过计时器一点一点追加到同一个 AI 气泡里。\n\n输入摘要：%1。\n\n这段文本特意包含多行内容和较长的中文句子，用来检查 QTextLayout 的换行、气泡高度刷新、滚动条最大值变化、以及列表 viewport bottom margin 是否正确。如果视觉上看到气泡逐字增长，就说明流式刷新链路已经跑通。")
                .arg(promptPreview),
        QStringLiteral("收到。这里返回一段足够长的假数据，重点是模拟端到端交互：用户按回车发送，消息立即进入列表；AI 气泡随后创建，并以短间隔持续扩展文本。\n\n你可以把这段当作压力样例：%1。后续接真实平台方案时，只需要把固定文本来源替换为接口 token 回调，保留当前模型更新、仓库更新和视图滚动逻辑即可。")
                .arg(promptPreview),
        QStringLiteral("本轮回复用于验证 AI 对话和日常聊天分离后的输入体验。AI 输入栏只保留文本输入区域，没有表情、图片、截图、历史、发送图标等日常聊天按钮；鼠标在输入文本区域会显示文本选取样式。\n\n关于你的输入：%1。\n\n这条回复会继续拉长一些，方便检查长回复在浅色、深色主题下的背景、文字颜色、选择高亮和悬浮输入栏上方的安全间距。")
                .arg(promptPreview)
    };

    return replies.at(QRandomGenerator::global()->bounded(replies.size()));
}

void AiChatConversationWidget::finishActiveAiReplyStream()
{
    if (m_streamTimer->isActive()) {
        m_streamTimer->stop();
    }

    if (!m_streamConversationId.isEmpty() && !m_streamMessageId.isEmpty() && !m_streamReplyText.isEmpty()) {
        AiChatRepository::instance().updateAiChatMessageText(m_streamConversationId,
                                                             m_streamMessageId,
                                                             m_streamReplyText);
        if (m_currentConversation.conversationId == m_streamConversationId) {
            m_messageModel->updateMessageText(m_streamMessageId, m_streamReplyText);
            m_messageView->scrollToBottom();
        }
    }

    m_streamConversationId.clear();
    m_streamMessageId.clear();
    m_streamReplyText.clear();
    m_streamOffset = 0;
}
