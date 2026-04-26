#include "AiChatListWidget.h"

#include <QApplication>
#include <QCursor>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QScrollBar>
#include <QStyleOptionViewItem>

#include "features/aichat/data/AiChatRepository.h"
#include "AiChatListDelegate.h"
#include "features/aichat/model/AiChatListModel.h"

AiChatListWidget::AiChatListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new AiChatListModel(this))
    , m_delegate(new AiChatListDelegate(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
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

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AiChatListWidget::onCurrentChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged, this, [this]() {
        updateStickyHeader();
        if (verticalScrollBar()->value() >= verticalScrollBar()->maximum() - 160) {
            loadMoreEntries();
        }
    });
    connect(m_model, &QAbstractItemModel::modelReset,
            this, [this]() { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsInserted,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::rowsRemoved,
            this, [this](const QModelIndex&, int, int) { updateStickyHeader(); });
    connect(m_model, &QAbstractItemModel::dataChanged,
            this, [this](const QModelIndex&, const QModelIndex&, const QVector<int>&) {
                updateStickyHeader();
            });
    updateStickyHeader();
}

void AiChatListWidget::ensureInitialized()
{
    if (m_initialized) {
        return;
    }

    m_initialized = true;
    reloadEntries();
    updateStickyHeader();
}

void AiChatListWidget::reloadEntries(const QString& selectedConversationId)
{
    m_nextOffset = 0;
    m_hasMore = true;
    loadMoreEntries();

    if (selectedConversationId.isEmpty()) {
        clearSelection();
        if (selectionModel()) {
            selectionModel()->clearCurrentIndex();
        }
        setCurrentIndex(QModelIndex());
        return;
    }

    while (m_model->rowOfConversation(selectedConversationId) < 0 && m_hasMore) {
        loadMoreEntries();
    }

    const int row = m_model->rowOfConversation(selectedConversationId);
    if (row < 0) {
        clearSelection();
        if (selectionModel()) {
            selectionModel()->clearCurrentIndex();
        }
        setCurrentIndex(QModelIndex());
        return;
    }

    selectionModel()->setCurrentIndex(m_model->index(row, 0),
                                      QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
}

void AiChatListWidget::loadMoreEntries()
{
    if (!m_hasMore) {
        return;
    }

    QVector<AiChatListEntry> entries = AiChatRepository::instance().requestAiChatList({
            m_nextOffset,
            kPageSize + 1
    });
    const bool hasMore = entries.size() > kPageSize;
    if (hasMore) {
        entries.resize(kPageSize);
    }

    if (m_nextOffset == 0) {
        m_model->setEntries(std::move(entries));
    } else {
        m_model->appendEntries(entries);
    }

    m_nextOffset += entries.size();
    m_hasMore = hasMore;
}

void AiChatListWidget::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        const QModelIndex index = indexAt(event->pos());
        if (index.isValid()) {
            QStyleOptionViewItem option = viewOptionForIndex(index);
            if (m_delegate->moreButtonRect(option, index).contains(event->pos())) {
                showItemMenu(index);
                event->accept();
                return;
            }
        }
    }

    OverlayScrollListView::mousePressEvent(event);
}

void AiChatListWidget::paintEvent(QPaintEvent* event)
{
    OverlayScrollListView::paintEvent(event);
    drawStickyHeader();
}

void AiChatListWidget::showEvent(QShowEvent* event)
{
    OverlayScrollListView::showEvent(event);
    ensureInitialized();
}

void AiChatListWidget::onCurrentChanged(const QModelIndex& current, const QModelIndex& previous)
{
    Q_UNUSED(previous);
    Q_UNUSED(current);
}

void AiChatListWidget::updateStickyHeader()
{
    const StickyHeaderState state = calculateStickyHeaderState();
    m_stickyVisible = state.visible;
    m_stickyOffsetY = state.offsetY;

    if (!m_stickyVisible) {
        m_stickyTitle.clear();
        viewport()->update();
        return;
    }

    m_stickyTitle = state.title;

    viewport()->update();
}

AiChatListWidget::StickyHeaderState AiChatListWidget::calculateStickyHeaderState() const
{
    StickyHeaderState state;
    if (m_model->rowCount() <= 0 || verticalScrollBar()->value() <= 0) {
        return state;
    }

    QModelIndex firstVisible = indexAt(QPoint(1, 1));
    if (!firstVisible.isValid()) {
        firstVisible = m_model->index(0, 0);
    }
    if (!firstVisible.isValid()) {
        return state;
    }

    const int currentSectionRow = m_model->sectionStartRowForRow(firstVisible.row());
    if (currentSectionRow < 0) {
        return state;
    }

    state.visible = true;
    state.title = m_model->sectionTitleAt(currentSectionRow);

    const int nextSectionRow = m_model->nextSectionStartRow(currentSectionRow);
    if (nextSectionRow >= 0) {
        const QRect nextRect = visualRect(m_model->index(nextSectionRow, 0));
        if (nextRect.isValid()) {
            state.offsetY = qMin(0, nextRect.top() - m_delegate->stickyHeaderHeight());
        }
    }

    return state;
}

QStyleOptionViewItem AiChatListWidget::viewOptionForIndex(const QModelIndex& index) const
{
    QStyleOptionViewItem option;
    initViewItemOption(&option);
    option.state |= QStyle::State_Enabled;
    if (selectionModel() && selectionModel()->currentIndex() == index) {
        option.state |= QStyle::State_Selected;
    }
    if (viewport()->rect().contains(viewport()->mapFromGlobal(QCursor::pos()))
        && indexAt(viewport()->mapFromGlobal(QCursor::pos())) == index) {
        option.state |= QStyle::State_MouseOver;
    }
    option.widget = viewport();
    option.rect = visualRect(index);
    return option;
}

void AiChatListWidget::showItemMenu(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    QStyleOptionViewItem option = viewOptionForIndex(index);
    const QRect buttonRect = m_delegate->moreButtonRect(option, index);

    QMenu menu(this);
    QAction* renameAction = menu.addAction(QStringLiteral("重命名"));
    QAction* deleteAction = menu.addAction(QStringLiteral("删除"));

    QAction* selectedAction = menu.exec(viewport()->mapToGlobal(buttonRect.bottomLeft()));
    if (selectedAction == renameAction) {
        renameItem(index);
    } else if (selectedAction == deleteAction) {
        deleteItem(index);
    }
}

void AiChatListWidget::renameItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const AiChatListEntry entry = m_model->entryAt(index);
    bool ok = false;
    const QString newTitle = QInputDialog::getText(this,
                                                   QStringLiteral("重命名"),
                                                   QStringLiteral("新标题："),
                                                   QLineEdit::Normal,
                                                   entry.title,
                                                   &ok).trimmed();
    if (!ok || newTitle.isEmpty()) {
        return;
    }

    if (AiChatRepository::instance().renameAiChatConversation(entry.conversationId, newTitle)) {
        reloadEntries(entry.conversationId);
    }
}

void AiChatListWidget::deleteItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const int removedRow = index.row();
    const bool wasCurrent = currentIndex() == index;
    const QString removedId = index.data(AiChatListModel::ConversationIdRole).toString();
    QString nextSelectionId;
    if (m_model->rowCount() > 1) {
        const int nextRow = qBound(0, removedRow, m_model->rowCount() - 2);
        const QModelIndex nextIndex = m_model->index(nextRow, 0);
        nextSelectionId = nextIndex.data(AiChatListModel::ConversationIdRole).toString();
        if (nextSelectionId == removedId && removedRow > 0) {
            nextSelectionId = m_model->index(removedRow - 1, 0).data(AiChatListModel::ConversationIdRole).toString();
        }
    }

    if (!AiChatRepository::instance().removeAiChatConversation(removedId)) {
        return;
    }

    reloadEntries(wasCurrent ? nextSelectionId
                             : currentIndex().data(AiChatListModel::ConversationIdRole).toString());
}

void AiChatListWidget::drawStickyHeader() const
{
    if (!m_stickyVisible || m_stickyTitle.isEmpty()) {
        return;
    }

    QPainter painter(viewport());
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setClipRect(QRect(0, m_stickyOffsetY, viewport()->width(), m_delegate->stickyHeaderHeight()));

    const QRect headerRect(0, m_stickyOffsetY, viewport()->width(), m_delegate->stickyHeaderHeight());
    painter.fillRect(headerRect, Qt::white);

    QFont headerFont = QApplication::font();
    headerFont.setPixelSize(13);
    headerFont.setBold(true);
    painter.setFont(headerFont);

    const QRect textRect(12, headerRect.top(), headerRect.width() - 24, headerRect.height());
    painter.setPen(QColor(0x66, 0x66, 0x66));
    painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, m_stickyTitle);
}
