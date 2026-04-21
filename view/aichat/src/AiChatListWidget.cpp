#include "AiChatListWidget.h"

#include <QApplication>
#include <QCursor>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLineEdit>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QScrollBar>
#include <QStyleOptionViewItem>
#include <QStringList>
#include <QVariantAnimation>

#include "AiChatListDelegate.h"
#include "AiChatListModel.h"

AiChatListWidget::AiChatListWidget(QWidget* parent)
    : OverlayScrollListView(parent)
    , m_model(new AiChatListModel(this))
    , m_delegate(new AiChatListDelegate(this))
    , m_stickyAnimation(new QVariantAnimation(this))
{
    setModel(m_model);
    setItemDelegate(m_delegate);
    setSelectionMode(QAbstractItemView::SingleSelection);
    setSelectionBehavior(QAbstractItemView::SelectRows);
    setUniformItemSizes(false);
    setSpacing(0);
    setEditTriggers(QAbstractItemView::NoEditTriggers);
    setStyleSheet("border-width:0px;border-style:solid;background:#ffffff;");
    setWheelStepPixels(64);
    setScrollBarInsets(8, 4);

    m_stickyAnimation->setDuration(180);
    m_stickyAnimation->setEasingCurve(QEasingCurve::OutCubic);
    connect(m_stickyAnimation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        m_stickyTransitionProgress = value.toReal();
        viewport()->update();
    });
    connect(m_stickyAnimation, &QVariantAnimation::finished, this, [this]() {
        m_previousStickyTitle.clear();
        m_stickyTransitionProgress = 1.0;
        viewport()->update();
    });

    connect(selectionModel(), &QItemSelectionModel::currentChanged,
            this, &AiChatListWidget::onCurrentChanged);
    connect(verticalScrollBar(), &QScrollBar::valueChanged,
            this, &AiChatListWidget::updateStickyHeader);
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

    const QStringList sampleTitles = {
        QStringLiteral("深度研究助理"),
        QStringLiteral("界面重构草案"),
        QStringLiteral("插件调试记录"),
        QStringLiteral("多代理协作实验"),
        QStringLiteral("日报自动整理"),
        QStringLiteral("提示词优化备份"),
        QStringLiteral("模型切换对照"),
        QStringLiteral("前端交互验证")
    };

    const QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < 40; ++i) {
        const int offsetDays = QRandomGenerator::global()->bounded(0, 18);
        const int offsetMinutes = QRandomGenerator::global()->bounded(0, 24 * 60);

        addChatItem(QStringLiteral("%1 %2")
                            .arg(sampleTitles.at(i % sampleTitles.size()))
                            .arg(i + 1),
                    now.addDays(-offsetDays).addSecs(-offsetMinutes * 60));
    }

    updateStickyHeader();
}

void AiChatListWidget::addChatItem(const QString& title, const QDateTime& time)
{
    if (title.isEmpty() || !time.isValid()) {
        return;
    }

    m_model->addEntry({title, time});
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
        m_stickyAnimation->stop();
        m_stickyTitle.clear();
        m_previousStickyTitle.clear();
        m_stickyTransitionProgress = 1.0;
        viewport()->update();
        return;
    }

    if (m_stickyTitle != state.title) {
        m_previousStickyTitle = m_stickyTitle;
        m_stickyTitle = state.title;

        if (m_previousStickyTitle.isEmpty()) {
            m_stickyTransitionProgress = 1.0;
            m_previousStickyTitle.clear();
        } else {
            m_stickyAnimation->stop();
            m_stickyTransitionProgress = 0.0;
            m_stickyAnimation->setStartValue(0.0);
            m_stickyAnimation->setEndValue(1.0);
            m_stickyAnimation->start();
        }
    }

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

    m_model->updateTitle(index.row(), newTitle);
}

void AiChatListWidget::deleteItem(const QModelIndex& index)
{
    if (!index.isValid()) {
        return;
    }

    const int removedRow = index.row();
    const bool wasCurrent = currentIndex() == index;

    m_model->removeRowAt(removedRow);

    if (wasCurrent) {
        if (m_model->rowCount() > 0) {
            const int nextRow = qBound(0, removedRow, m_model->rowCount() - 1);
            selectionModel()->setCurrentIndex(m_model->index(nextRow, 0),
                                              QItemSelectionModel::ClearAndSelect | QItemSelectionModel::Rows);
        } else {
            clearSelection();
            setCurrentIndex(QModelIndex());
        }
    }
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
    const int slideDistance = 10;

    auto drawText = [&painter, &textRect](const QString& text, int offsetY, qreal opacity) {
        if (text.isEmpty() || opacity <= 0.0) {
            return;
        }

        painter.save();
        painter.setOpacity(opacity);
        painter.setPen(QColor(0x66, 0x66, 0x66));
        painter.drawText(textRect.translated(0, offsetY), Qt::AlignLeft | Qt::AlignVCenter, text);
        painter.restore();
    };

    if (!m_previousStickyTitle.isEmpty()) {
        drawText(m_previousStickyTitle,
                 -qRound(m_stickyTransitionProgress * slideDistance),
                 1.0 - m_stickyTransitionProgress);
        drawText(m_stickyTitle,
                 qRound((1.0 - m_stickyTransitionProgress) * slideDistance),
                 m_stickyTransitionProgress);
        return;
    }

    drawText(m_stickyTitle, 0, 1.0);
}
