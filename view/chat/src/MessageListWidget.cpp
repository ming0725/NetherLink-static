#include "MessageListWidget.h"

#include <QItemSelectionModel>

#include "MessageListDelegate.h"
#include "MessageListModel.h"
#include "MessageRepository.h"

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
    setStyleSheet("border-width:0px;border-style:solid;background:#ffffff;");
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    m_model->setConversations(MessageRepository::instance().requestConversationList());

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MessageListWidget::onCurrentChanged);
}

QString MessageListWidget::selectedConversationId() const
{
    return m_model->conversationIdAt(currentIndex());
}

ConversationSummary MessageListWidget::selectedConversation() const
{
    return m_model->conversationAt(currentIndex());
}

void MessageListWidget::onLastMessageUpdated(const QString& chatId,
                                             const QString& text,
                                             const QDateTime& timestamp)
{
    const QString selectedId = selectedConversationId();
    m_model->updateConversationPreview(chatId, text, timestamp);
    restoreSelection(selectedId);
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

void MessageListWidget::restoreSelection(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        clearSelection();
        return;
    }

    const int row = m_model->indexOfConversation(conversationId);
    if (row < 0) {
        clearSelection();
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    m_restoringSelection = true;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_restoringSelection = false;
}
