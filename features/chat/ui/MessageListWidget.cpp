#include "MessageListWidget.h"

#include <QAction>
#include <QItemSelectionModel>
#include <QMouseEvent>
#include <QPalette>
#include <QScrollBar>
#include <QTimer>

#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"
#include "features/friend/data/UserRepository.h"
#include "features/chat/ui/MessageListDelegate.h"
#include "features/chat/model/MessageListModel.h"
#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"

MessageListWidget::MessageListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new MessageListModel(this))
    , m_delegate(new MessageListDelegate(this))
    , m_searchDebounceTimer(new QTimer(this))
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
    palette.setColor(QPalette::Base, ThemeManager::instance().color(ThemeColor::PanelBackground));
    palette.setColor(QPalette::Window, ThemeManager::instance().color(ThemeColor::PanelBackground));
    setPalette(palette);
    viewport()->setPalette(palette);
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    m_searchDebounceTimer->setSingleShot(true);
    m_searchDebounceTimer->setInterval(180);
    connect(m_searchDebounceTimer, &QTimer::timeout,
            this, &MessageListWidget::reloadConversations);

    m_model->setConversations(MessageRepository::instance().requestConversationList());

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &MessageListWidget::onCurrentChanged);
    connect(this, &QAbstractItemView::pressed, this, [this](const QModelIndex& index) {
        const QString conversationId = m_model->conversationIdAt(index);
        if (conversationId.isEmpty()) {
            return;
        }

        m_model->markConversationRead(conversationId);
        MessageRepository::instance().markConversationRead(conversationId);
        viewport()->update();
    });
    connect(&MessageRepository::instance(), &MessageRepository::lastMessageChanged,
            this, &MessageListWidget::onRepositoryLastMessageChanged);
    connect(&MessageRepository::instance(), &MessageRepository::conversationListChanged,
            this, &MessageListWidget::reloadConversations);
    connect(&UserRepository::instance(), &UserRepository::friendListChanged,
            this, &MessageListWidget::reloadConversations);
    connect(&GroupRepository::instance(), &GroupRepository::groupListChanged,
            this, &MessageListWidget::reloadConversations);
    QTimer::singleShot(0, this, [this]() { clearCurrentConversationSelection(); });
}

void MessageListWidget::setKeyword(const QString& keyword)
{
    if (m_state.keyword == keyword) {
        return;
    }

    m_state.keyword = keyword;
    m_searchDebounceTimer->start();
}

QString MessageListWidget::selectedConversationId() const
{
    return m_model->conversationIdAt(currentIndex());
}

ConversationSummary MessageListWidget::selectedConversation() const
{
    return m_model->conversationAt(currentIndex());
}

void MessageListWidget::setCurrentConversation(const QString& conversationId)
{
    if (conversationId.isEmpty() || !selectionModel()) {
        return;
    }

    const int row = m_model->indexOfConversation(conversationId);
    if (row < 0) {
        clearCurrentConversationSelection();
        return;
    }

    const QModelIndex index = m_model->index(row, 0);
    m_model->markConversationRead(conversationId);
    MessageRepository::instance().markConversationRead(conversationId);
    m_restoringSelection = true;
    selectionModel()->setCurrentIndex(index, QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
    m_restoringSelection = false;
    scrollTo(index);
    viewport()->update();
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

void MessageListWidget::mousePressEvent(QMouseEvent* event)
{
    const QModelIndex pressedIndex = indexAt(event->pos());
    if (event->button() == Qt::RightButton) {
        if (pressedIndex.isValid()) {
            showConversationMenu(event->globalPosition().toPoint(), pressedIndex);
        }
        event->accept();
        return;
    }

    OverlayScrollListView::mousePressEvent(event);
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
    MessageRepository::instance().markConversationRead(conversationId);
    update(current);

    if (!m_restoringSelection && conversationId != m_model->conversationIdAt(previous)) {
        emit conversationActivated(conversationId);
    }
}

void MessageListWidget::onRepositoryLastMessageChanged(const QString& conversationId,
                                                       QSharedPointer<ChatMessage> lastMessage)
{
    m_model->updateConversationPreview(conversationId,
                                       previewTextForMessage(conversationId, lastMessage),
                                       lastMessage ? lastMessage->getTimestamp() : QDateTime());
}

void MessageListWidget::showConversationMenu(const QPoint& globalPos, const QModelIndex& index)
{
    const ConversationSummary conversation = m_model->conversationAt(index);
    if (conversation.conversationId.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(QColor(238, 238, 238));
    m_model->setContextMenuConversation(conversation.conversationId);

    QAction* pinAction = menu->addAction(conversation.isPinned
                                         ? QStringLiteral("取消置顶")
                                         : QStringLiteral("置顶"));
    connect(pinAction, &QAction::triggered, this,
            [this, conversationId = conversation.conversationId,
             pinned = !conversation.isPinned]() {
        MessageRepository::instance().setConversationPinned(conversationId, pinned);
        const int row = m_model->indexOfConversation(conversationId);
        if (row >= 0) {
            scrollTo(m_model->index(row, 0));
        }
        viewport()->update();
    });

    const bool hasUnread = conversation.unreadCount > 0;
    QAction* unreadAction = menu->addAction(hasUnread
                                            ? QStringLiteral("标记已读")
                                            : QStringLiteral("标记未读"));
    connect(unreadAction, &QAction::triggered, this,
            [this, conversationId = conversation.conversationId, hasUnread]() {
        if (hasUnread) {
            m_model->markConversationRead(conversationId);
            MessageRepository::instance().markConversationRead(conversationId);
            viewport()->update();
            return;
        }

        m_model->markConversationUnread(conversationId);
        MessageRepository::instance().markConversationUnread(conversationId);
        viewport()->update();
    });

    QAction* dndAction = menu->addAction(conversation.isDoNotDisturb
                                         ? QStringLiteral("取消免打扰")
                                         : QStringLiteral("消息免打扰"));
    connect(dndAction, &QAction::triggered, this,
            [this, conversationId = conversation.conversationId,
             enabled = !conversation.isDoNotDisturb]() {
        m_model->setConversationDoNotDisturb(conversationId, enabled);
        MessageRepository::instance().setConversationDoNotDisturb(conversationId, enabled);
        viewport()->update();
    });

    menu->addSeparator();

    QAction* deleteAction = menu->addAction(QStringLiteral("删除"));
    StyledActionMenu::setActionColors(deleteAction,
                                      QColor(235, 87, 87),
                                      QColor(255, 255, 255),
                                      QColor(235, 87, 87));
    connect(deleteAction, &QAction::triggered, this,
            [this, conversationId = conversation.conversationId]() {
        const bool deletingSelected = selectedConversationId() == conversationId;
        if (deletingSelected) {
            clearCurrentConversationSelection();
        }
        m_model->removeConversation(conversationId);
        MessageRepository::instance().removeConversation(conversationId);
        if (deletingSelected) {
            emit currentConversationDeleted();
        }
        viewport()->update();
    });

    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        m_model->setContextMenuConversation({});
        viewport()->update();
        menu->deleteLater();
    });
    // 和聊天气泡菜单保持一致：等右键 release 后再打开原生菜单，避免跨区域鼠标状态残留。
    menu->popupWhenMouseReleased(globalPos);
}

void MessageListWidget::reloadConversations()
{
    const QString selectedId = selectedConversationId();
    const int scrollValue = verticalScrollBar() ? verticalScrollBar()->value() : 0;
    m_model->setConversations(MessageRepository::instance().requestConversationList({m_state.keyword}));
    restoreSelection(selectedId);
    if (verticalScrollBar()) {
        verticalScrollBar()->setValue(scrollValue);
    }
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
