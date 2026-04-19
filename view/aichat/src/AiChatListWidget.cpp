// AiChatListWidget.cpp
#include "AiChatListWidget.h"
#include "CustomScrollArea.h"
#include "AiChatListItem.h"
#include <QLabel>
#include <QPushButton>
#include <QDateTime>
#include <algorithm>
#include <QRandomGenerator>

AiChatListWidget::AiChatListWidget(QWidget* parent)
        : CustomScrollArea(parent)
{
    // 将滚动区的 viewport 作为内容容器
    content = contentWidget;
    content->setMouseTracking(true);
    setStyleSheet("border-width:0px;border-style:solid;");

    // 随机生成 40 条示例对话
    QDateTime now = QDateTime::currentDateTime();
    for (int i = 0; i < 40; ++i) {
        // 随机生成 0~365 天的偏移
        int offsetDays = QRandomGenerator::global()->bounded(0, 366);
        QDateTime dt = now.addDays(-offsetDays);

        AiChatListItem* item = new AiChatListItem(content);
        // 标题中附加日期，格式如 "示例对话 1 2025-05-19"
        QString dateStr = dt.toString("yyyy-MM-dd");
        item->setTitle(QString("示例对话 %1").arg(dateStr));
        item->setTime(dt);

        addChatItem(item);
    }

    layoutContent();
}


void AiChatListWidget::addChatItem(AiChatListItem* item) {
    m_items.append(item);
    connect(item, &AiChatListItem::clicked,
            this, &AiChatListWidget::onItemClicked);
    connect(item, &AiChatListItem::requestRename, this,
            [this](AiChatListItem* it){ emit chatItemRenamed(it); });
    connect(item, &AiChatListItem::requestDelete, this,
            [this](AiChatListItem* it){ emit chatItemDeleted(it); });
    connect(item, &AiChatListItem::timeUpdated, this,
            [this](){ emit chatOrderChanged(); });
}

QString AiChatListWidget::timeSectionText(const QDateTime& dt) const {
    QDateTime now = QDateTime::currentDateTime();
    int days = dt.date().daysTo(now.date());
    if (dt.date() == now.date())
        return QStringLiteral("今天");
    else if (dt.date().addDays(1) == now.date())
        return QStringLiteral("昨天");
    else if (days <= 7)
        return QStringLiteral("七天内");
    else if (days <= 30)
        return QStringLiteral("一个月内");
    else
        return dt.toString("yyyy-MM");
}

void AiChatListWidget::layoutContent() {
    // 1. 按时间降序排序
    std::sort(m_items.begin(), m_items.end(),
              [](AiChatListItem* a, AiChatListItem* b){
                  return a->time() > b->time();
              });

    // 3. 单次遍历，插标签 + item
    int y = topMargin;
    const int W = content->width();
    QString lastSec;  // 上一次插入的分组

    for (auto* item : m_items) {
        QString sec = timeSectionText(item->time());
        // 不同分组就先插入一个加粗的 label
        if (sec != lastSec) {
            QLabel* lbl = new QLabel(sec, content);
            QFont f = lbl->font();
            f.setPixelSize(14);
            f.setStyleStrategy(QFont::PreferAntialias);
            lbl->setFont(f);
            lbl->setGeometry(leftMargin, y + groupSpacing, W - 2*leftMargin, lbl->height());
            y += lbl->height() + sectionSpacing + groupSpacing;
            lastSec = sec;
        }
        int h = item->height();
        item->setGeometry(leftMargin, y, W - 2*leftMargin, h);
        y += h + itemSpacing;
    }
    // 4. 调整内容总高度
    content->resize(W, y + topMargin);
}

void AiChatListWidget::onItemClicked(AiChatListItem* item) {
    if (!item) return;
    if (selectedItem) {
        selectedItem->setSelected(false);
        item->setSelected(true);
        selectedItem = item;
    }
    else {
        selectedItem = item;
        selectedItem->setSelected(true);
    }
}
