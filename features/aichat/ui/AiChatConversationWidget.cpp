#include "AiChatConversationWidget.h"

#include <QLinearGradient>
#include <QModelIndex>
#include <QPainter>
#include <QResizeEvent>
#include <QTimer>

#include "features/aichat/model/AiChatMessageListModel.h"
#include "features/aichat/ui/AiChatFloatingInputBar.h"
#include "features/aichat/ui/AiChatMessageListView.h"
#include "features/aichat/ui/AiChatSessionController.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/PaintedLabel.h"

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
    , m_titleLabel(new PaintedLabel(this))
    , m_headerDivider(new ThemeDivider(this))
    , m_bottomGapGradientOverlay(new BottomGapGradientOverlay(this))
    , m_emptyLabel(new PaintedLabel(QStringLiteral("选择或新建一个 AI 对话"), this))
{
    m_messageView->setModel(m_messageModel);
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
    connect(m_inputBar, &AiChatFloatingInputBar::stopStreamingRequested,
            this, &AiChatConversationWidget::onStopStreamingRequested);
    connect(m_inputBar, &AiChatFloatingInputBar::inputFocused,
            m_messageView, &AiChatMessageListView::clearTextSelection);
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        updateHeader();
        update();
        m_headerDivider->update();
        m_bottomGapGradientOverlay->update();
        m_messageView->viewport()->update();
    });

    closeConversation();
}

void AiChatConversationWidget::setController(AiChatSessionController* controller)
{
    if (m_controller == controller) {
        return;
    }

    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;
    if (!m_controller) {
        closeConversation();
        return;
    }

    connect(m_controller, &AiChatSessionController::aiReplyStarted,
            this, &AiChatConversationWidget::onAiReplyStarted);
    connect(m_controller, &AiChatSessionController::aiReplyMessageAdded,
            this, &AiChatConversationWidget::onAiReplyMessageAdded);
    connect(m_controller, &AiChatSessionController::aiReplyMessageUpdated,
            this, &AiChatConversationWidget::onAiReplyMessageUpdated);
    connect(m_controller, &AiChatSessionController::aiReplyFinished,
            this, &AiChatConversationWidget::onAiReplyFinished);
    connect(m_controller, &AiChatSessionController::aiReplyCanceled,
            this, &AiChatConversationWidget::onAiReplyCanceled);
}

void AiChatConversationWidget::openConversation(const AiChatListEntry& entry)
{
    if (entry.conversationId.isEmpty()) {
        closeConversation();
        return;
    }

    cancelActiveAiReplyStream();
    m_currentConversation = entry;
    m_messageModel->setMessages(m_controller ? m_controller->loadMessages(entry.conversationId) : QVector<AiChatMessage>{});
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
    cancelActiveAiReplyStream();
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
    if (!m_controller || m_currentConversation.conversationId.isEmpty() || hasActiveAiReplyStream()) {
        return;
    }

    const AiChatMessage message = m_controller->submitUserMessage(m_currentConversation.conversationId, text);
    if (message.messageId.isEmpty()) {
        return;
    }

    m_messageModel->appendMessage(message);
    m_messageView->clearTextSelection();
    m_messageView->scrollToBottom();
}

void AiChatConversationWidget::onStopStreamingRequested()
{
    cancelActiveAiReplyStream();
}

void AiChatConversationWidget::updateLayout()
{
    const int titleY = kHeaderHeight - kHeaderTitleBottomMargin - kHeaderTitleHeight;
    m_titleLabel->setGeometry(kHeaderTitleLeft,
                              titleY,
                              qMax(0, width() - kHeaderTitleLeft - kHeaderTitleRight),
                              kHeaderTitleHeight);
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
    m_bottomGapGradientOverlay->hide();

    m_inputBar->raise();
}

void AiChatConversationWidget::updateHeader()
{
    const bool hasConversation = !m_currentConversation.conversationId.isEmpty();
    m_titleLabel->setText(hasConversation ? m_currentConversation.title : QStringLiteral("AI 对话"));
    m_titleLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    m_emptyLabel->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
}

void AiChatConversationWidget::onAiReplyStarted(const QString& conversationId)
{
    if (m_currentConversation.conversationId == conversationId) {
        m_inputBar->setStreaming(true);
    }
}

void AiChatConversationWidget::onAiReplyMessageAdded(const AiChatMessage& message)
{
    if (m_currentConversation.conversationId == message.conversationId) {
        m_messageModel->appendMessage(message);
        m_messageView->scrollToBottom();
        return;
    }
}

void AiChatConversationWidget::onAiReplyMessageUpdated(const QString& conversationId,
                                                       const QString& messageId,
                                                       const QString& text)
{
    if (m_currentConversation.conversationId == conversationId) {
        m_messageModel->updateMessageText(messageId, text);
        m_messageView->scrollToBottom();
    }
}

void AiChatConversationWidget::onAiReplyFinished(const QString& conversationId, const QString& messageId)
{
    Q_UNUSED(messageId);
    if (m_currentConversation.conversationId == conversationId) {
        m_inputBar->setStreaming(false);
    }
}

void AiChatConversationWidget::onAiReplyCanceled(const QString& conversationId, const QString& messageId)
{
    Q_UNUSED(messageId);
    if (m_currentConversation.conversationId == conversationId || m_currentConversation.conversationId.isEmpty()) {
        m_inputBar->setStreaming(false);
    }
}

void AiChatConversationWidget::cancelActiveAiReplyStream()
{
    if (m_controller) {
        m_controller->cancelActiveAiReplyStream();
    }
    if (m_inputBar) {
        m_inputBar->setStreaming(false);
    }
}

bool AiChatConversationWidget::hasActiveAiReplyStream() const
{
    return m_controller && m_controller->hasActiveAiReplyStream();
}
