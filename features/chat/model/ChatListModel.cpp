#include "ChatListModel.h"
#include <QDateTime>
#include <QDebug>
#include <QSize>

Q_DECLARE_METATYPE(TimeHeader*)
Q_DECLARE_METATYPE(ChatMessage*)

ChatListModel::ChatListModel(QObject* parent)
    : QAbstractListModel(parent)
{
    qRegisterMetaType<TimeHeader*>();
    qRegisterMetaType<ChatMessage*>();
}

int ChatListModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(items.size());
}

QVariant ChatListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items.size()))
        return QVariant();

    const ListItem& item = items[index.row()];
    
    if (role == Qt::UserRole) {
        if (item.isHeader) {
            return QVariant::fromValue<TimeHeader*>(item.timeHeader.get());
        } else if (item.isBottomSpace) {
            return QVariant::fromValue<int>(item.bottomSpaceHeight);
        } else {
            return QVariant::fromValue<ChatMessage*>(item.message.get());
        }
    } else if (role == Qt::SizeHintRole && item.isBottomSpace) {
        return QSize(0, item.bottomSpaceHeight);
    }
    
    return QVariant();
}

bool ChatListModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= static_cast<int>(items.size()))
        return false;

    if (role == Qt::UserRole + 1 &&
        !items[index.row()].isHeader &&
        !items[index.row()].isBottomSpace &&
        items[index.row()].message) {
        bool selected = value.toBool();
        if (selected && selectedMessageIndex != index.row()) {
            // 清除之前选中的消息
            if (selectedMessageIndex >= 0) {
                items[selectedMessageIndex].message->setSelected(false);
                QModelIndex prevIndex = this->index(selectedMessageIndex);
                emit dataChanged(prevIndex, prevIndex);
            }
            // 设置新选中的消息
            items[index.row()].message->setSelected(true);
            selectedMessageIndex = index.row();
        } else if (!selected) {
            // 清除选中状态
            if (selectedMessageIndex >= 0) {
                items[selectedMessageIndex].message->setSelected(false);
                selectedMessageIndex = -1;
            }
        }
        emit dataChanged(index, index);
        return true;
    }
    return false;
}

Qt::ItemFlags ChatListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid())
        return Qt::NoItemFlags;
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void ChatListModel::addMessage(QSharedPointer<ChatMessage> message)
{
    // 确保消息有有效的时间戳
    if (!message || !message->getTimestamp().isValid()) {
        qDebug() << "Warning: Attempting to add message with invalid timestamp";
        return;
    }

    beginResetModel();
    messages.push_back(std::move(message));
    rebuildItems();
    endResetModel();
}

void ChatListModel::setMessages(QVector<QSharedPointer<ChatMessage>> nextMessages)
{
    beginResetModel();
    messages = std::move(nextMessages);
    rebuildItems();
    endResetModel();
}

void ChatListModel::prependMessages(QVector<QSharedPointer<ChatMessage>> olderMessages)
{
    if (olderMessages.isEmpty()) {
        return;
    }

    beginResetModel();
    messages = olderMessages + messages;
    rebuildItems();
    endResetModel();
}

const ChatMessage* ChatListModel::messageAt(int index) const
{
    if (index >= 0 &&
        index < static_cast<int>(items.size()) &&
        !items[index].isHeader &&
        !items[index].isBottomSpace) {
        return items[index].message.get();
    }
    return nullptr;
}

QSharedPointer<ChatMessage> ChatListModel::sharedMessageAt(int index) const
{
    if (index >= 0 &&
        index < static_cast<int>(items.size()) &&
        !items[index].isHeader &&
        !items[index].isBottomSpace) {
        return items[index].message;
    }
    return {};
}

void ChatListModel::clearSelection()
{
    if (selectedMessageIndex >= 0) {
        items[selectedMessageIndex].message->setSelected(false);
        QModelIndex index = this->index(selectedMessageIndex);
        selectedMessageIndex = -1;
        emit dataChanged(index, index);
    }
}

bool ChatListModel::removeMessage(int index)
{
    if (index < 0 || index >= static_cast<int>(items.size())) {
        return false;
    }

    // 如果要删除的是底部空白或时间标识，直接返回
    if (items[index].isBottomSpace || items[index].isHeader) {
        return false;
    }

    const int messageIndex = messageIndexForRow(index);
    if (messageIndex < 0 || messageIndex >= messages.size()) {
        return false;
    }

    beginResetModel();
    messages.removeAt(messageIndex);
    selectedMessageIndex = -1;
    rebuildItems();
    endResetModel();
    return true;
}

bool ChatListModel::isTimeHeader(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size()))
        return items[index].isHeader;
    return false;
}

const TimeHeader* ChatListModel::getTimeHeader(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size()) && items[index].isHeader)
        return items[index].timeHeader.get();
    return nullptr;
}

bool ChatListModel::shouldAddTimeHeader(const QDateTime& prevTime, const QDateTime& currTime) const
{
    // 计算时间间隔（秒）
    qint64 interval = prevTime.secsTo(currTime);
    
    // 如果时间间隔为负数（可能是由于系统时间调整），则添加时间标识
    if (interval < 0) {
        interval = -interval;  // 使用绝对值
    }
    
    // 如果时间间隔超过设定值，则添加时间标识
    return interval >= TimeSettings::MESSAGE_TIME_INTERVAL;
}

TimeHeaderType ChatListModel::getTimeHeaderType(const QDateTime& timestamp) const
{
    QDateTime now = QDateTime::currentDateTime();
    qint64 daysTo = timestamp.daysTo(now);

    if (daysTo == 0) {
        return TimeHeaderType::Time;
    } else if (daysTo == 1) {
        return TimeHeaderType::Yesterday;
    } else if (daysTo <= 7) {
        return TimeHeaderType::DayOfWeek;
    } else if (timestamp.date().year() == now.date().year()) {
        return TimeHeaderType::ThisYear;
    } else {
        return TimeHeaderType::FullDate;
    }
}

QString ChatListModel::formatTimeHeader(const QDateTime& timestamp) const
{
    QString timeStr = timestamp.toString("HH:mm");
    
    switch (getTimeHeaderType(timestamp)) {
        case TimeHeaderType::Time:
            return timeStr;
        case TimeHeaderType::Yesterday:
            return QString("昨天 %1").arg(timeStr);
        case TimeHeaderType::DayOfWeek: {
            static const QStringList weekDays = {"周日", "周一", "周二", "周三", "周四", "周五", "周六"};
            return QString("%1 %2").arg(weekDays[timestamp.date().dayOfWeek() % 7], timeStr);
        }
        case TimeHeaderType::ThisYear:
            return timestamp.toString("MM-dd HH:mm");
        case TimeHeaderType::FullDate:
            return timestamp.toString("yyyy-MM-dd HH:mm");
    }
    return QString();
}

bool ChatListModel::isBottomSpace(int index) const
{
    if (index >= 0 && index < static_cast<int>(items.size())) {
        return items[index].isBottomSpace;
    }
    return false;
}

void ChatListModel::ensureBottomSpace()
{
    // 如果没有底部空白，添加一个
    if (items.empty() || !items.back().isBottomSpace) {
        beginInsertRows(QModelIndex(), items.size(), items.size());
        ListItem bottomSpace;
        bottomSpace.isBottomSpace = true;
        bottomSpace.bottomSpaceHeight = bottomSpaceHeight;
        items.push_back(std::move(bottomSpace));
        endInsertRows();
    }
}

void ChatListModel::setBottomSpaceHeight(int height)
{
    bottomSpaceHeight = height;
    // 更新底部空白的高度
    if (!items.empty() && items.back().isBottomSpace) {
        items.back().bottomSpaceHeight = bottomSpaceHeight;
        QModelIndex lastIndex = index(items.size() - 1, 0);
        emit dataChanged(lastIndex, lastIndex);
    }
}

void ChatListModel::clear() {
    beginResetModel();
    items.clear();
    messages.clear();
    selectedMessageIndex = -1;
    endResetModel();
}

void ChatListModel::rebuildItems()
{
    items.clear();
    selectedMessageIndex = -1;

    QDateTime previousTime;
    for (const QSharedPointer<ChatMessage>& message : messages) {
        if (!message || !message->getTimestamp().isValid()) {
            continue;
        }

        const bool needTimeHeader = !previousTime.isValid() ||
                shouldAddTimeHeader(previousTime, message->getTimestamp());
        if (needTimeHeader) {
            ListItem timeItem;
            timeItem.isHeader = true;
            timeItem.timeHeader = QSharedPointer<TimeHeader>::create();
            timeItem.timeHeader->timestamp = message->getTimestamp();
            timeItem.timeHeader->type = getTimeHeaderType(message->getTimestamp());
            timeItem.timeHeader->text = formatTimeHeader(message->getTimestamp());
            items.push_back(std::move(timeItem));
        }

        ListItem messageItem;
        messageItem.message = message;
        items.push_back(std::move(messageItem));
        previousTime = message->getTimestamp();
    }

    ListItem bottomSpace;
    bottomSpace.isBottomSpace = true;
    bottomSpace.bottomSpaceHeight = bottomSpaceHeight;
    items.push_back(std::move(bottomSpace));
}

int ChatListModel::messageIndexForRow(int row) const
{
    if (row < 0 || row >= items.size() || items.at(row).isHeader || items.at(row).isBottomSpace) {
        return -1;
    }

    const ChatMessage* target = items.at(row).message.get();
    for (int index = 0; index < messages.size(); ++index) {
        if (messages.at(index).get() == target) {
            return index;
        }
    }
    return -1;
}
