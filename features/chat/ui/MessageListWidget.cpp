#include "MessageListWidget.h"

#include <QItemSelectionModel>
#include <QPalette>
#include <QTimer>

#include "features/friend/data/UserRepository.h"
#include "features/chat/ui/MessageListDelegate.h"
#include "features/chat/model/MessageListModel.h"
#include "features/chat/data/MessageRepository.h"

MessageListWidget::MessageListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new MessageListModel(this))
    , m_delegate(new MessageListDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(true);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setAutoFillBackground(true);
    viewport()->setAutoFillBackground(true);
    QPalette palette = this->palette();
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::Window, Qt::white);
    setPalette(palette);
    viewport()->setPalette(palette);
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    m_model->setConversations(MessageRepository::instance().requestConversationList());

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MessageListWidget::onCurrentChanged);
    connect(&MessageRepository::instance(), &MessageRepository::lastMessageChanged,
            this, &MessageListWidget::onRepositoryLastMessageChanged);
    QTimer::singleShot(0, this, [this]() { clearCurrentConversationSelection(); });
}

QString MessageListWidget::selectedConversationId() const
{
    return m_model->conversationIdAt(currentIndex());
}

ConversationSummary MessageListWidget::selectedConversation() const
{
    return m_model->conversationAt(currentIndex());
}

void MessageListWidget::clearCurrentConversationSelection()
{
    if (!selectionModel()) {
        return;
    }

    m_restoringSelection = true;
    clearSelection();
    selectionModel()->clearCurrentIndex();
    setCurrentIndex(QModelIndex());
    m_restoringSelection = false;
    viewport()->update();
}

void MessageListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    if (!current.isValid()) {
        return;
    }

    const QString conversationId = m_model->conversationIdAt(current);
    if (conversationId.isEmpty()) {
        return;
    }

    m_model->markConversationRead(conversationId);
    update(current);

    if (!m_restoringSelection && conversationId != m_model->conversationIdAt(previous)) {
        emit conversationActivated(conversationId);
    }
}

void MessageListWidget::onRepositoryLastMessageChanged(const QString& conversationId,
                                                       QSharedPointer<ChatMessage> lastMessage)
{
    const QString selectedId = selectedConversationId();
    m_model->updateConversationPreview(conversationId,
                                       previewTextForMessage(conversationId, lastMessage),
                                       lastMessage ? lastMessage->getTimestamp() : QDateTime());
    restoreSelection(selectedId);
}

void MessageListWidget::restoreSelection(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        clearCurrentConversationSelection();
        return;
    }

    const int row = m_model->indexOfConversation(conversationId);
    if (row < 0) {
        clearCurrentConversationSelection();
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    m_restoringSelection = true;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_restoringSelection = false;
}

QString MessageListWidget::previewTextForMessage(const QString& conversationId,
                                                 const QSharedPointer<ChatMessage>& message) const
{
    if (!message) {
        return {};
    }

    const ConversationSummary summary = m_model->conversationById(conversationId);
    if (!summary.isGroup) {
        return message->getContent();
    }

    QString senderName = message->getSenderName();
    if (senderName.isEmpty()) {
        senderName = UserRepository::instance().requestUserName(message->getSenderId());
    }
    return QString("%1：%2").arg(senderName, message->getContent());
}
