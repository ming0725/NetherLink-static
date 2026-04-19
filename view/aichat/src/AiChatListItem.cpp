#include "AiChatListItem.h"

#include <QPainter>
#include <QMouseEvent>
#include <QHBoxLayout>
#include <QCursor>
#include <QInputDialog>

AiChatListItem::AiChatListItem(QWidget* parent)
        : QWidget(parent)
{
    setCursor(Qt::PointingHandCursor);
    setFixedHeight(34);
    setMouseTracking(true);

    m_titleLabel = new QLabel(this);
    m_titleLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
    m_titleLabel->setStyleSheet("font-size: 14px; padding-left: 10px;");
    m_titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);

    m_moreButton = new QPushButton(this);
    m_moreButton->setIcon(QIcon(":/resources/icon/more.png"));
    m_moreButton->setFixedSize(24, 24);
    m_moreButton->setFlat(true);
    m_moreButton->setCursor(Qt::PointingHandCursor);
    m_moreButton->setVisible(false);
    m_moreButton->setStyleSheet("border: none;");

    QHBoxLayout* layout = new QHBoxLayout(this);
    layout->setContentsMargins(5, 0, 5, 0);
    layout->addWidget(m_titleLabel);
    layout->addWidget(m_moreButton);
    setLayout(layout);

    initMenu();
}

void AiChatListItem::initMenu()
{
    m_menu = new QMenu(this);
    QAction* renameAction = m_menu->addAction("重命名");
    QAction* deleteAction = m_menu->addAction("删除");

    connect(renameAction, &QAction::triggered, [this]() {
        bool ok = false;
        QString newTitle = QInputDialog::getText(this, "重命名", "新标题：", QLineEdit::Normal, m_title, &ok);
        if (ok && !newTitle.isEmpty()) {
            setTitle(newTitle);
            emit requestRename(this);
        }
    });

    connect(deleteAction, &QAction::triggered, [this]() {
        emit requestDelete(this);
    });
}

void AiChatListItem::setTitle(const QString& title)
{
    m_title = title;
    m_titleLabel->setText(m_title);
}

QString AiChatListItem::title() const
{
    return m_title;
}

void AiChatListItem::setTime(const QDateTime& time)
{
    m_time = time;
    emit timeUpdated(this);
}

QDateTime AiChatListItem::time() const
{
    return m_time;
}

bool AiChatListItem::operator<(const AiChatListItem& other) const
{
    return m_time < other.m_time;
}

void AiChatListItem::enterEvent(QEnterEvent*)
{
    m_hovered = true;
    update();
}

void AiChatListItem::leaveEvent(QEvent*)
{
    m_hovered = false;
    update();
}

void AiChatListItem::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        emit clicked(this);
    }
}

void AiChatListItem::paintEvent(QPaintEvent*)
{
    QPainter painter(this);
    QColor bgColor = Qt::white;
    if (m_pressed)
        bgColor = QColor(0xECECEC);
    else if (m_hovered)
        bgColor = QColor(0xF9F9F9);

    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(bgColor);
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(rect(), 8, 8);
    m_moreButton->setVisible(m_hovered || m_pressed);
}

void AiChatListItem::resizeEvent(QResizeEvent*)
{
    int maxTextWidth = width() - 30;
    QFontMetrics metrics(m_titleLabel->font());
    QString elided = metrics.elidedText(m_title, Qt::ElideRight, maxTextWidth);
    m_titleLabel->setText(elided);
}

void AiChatListItem::setSelected(bool select) {
    m_pressed = select;
    update();
}

bool AiChatListItem::isSelected() {
    return m_pressed;
}
