#include "AiChatListModel.h"

#include <QSize>

#include <algorithm>

AiChatListModel::AiChatListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int AiChatListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_entries.size());
}

QVariant AiChatListModel::data(const QModelIndex& index, int role) const
{
    const AiChatListEntry entry = entryAt(index);
    if (!entry.time.isValid() && entry.title.isEmpty() && entry.conversationId.isEmpty()) {
        return {};
    }

    switch (role) {
    case ConversationIdRole:
        return entry.conversationId;
    case Qt::DisplayRole:
    case TitleRole:
        return entry.title;
    case TimeRole:
        return entry.time;
    case SectionTitleRole:
        return sectionTitleAt(index.row());
    case IsSectionStartRole:
        return isSectionStart(index.row());
    case Qt::SizeHintRole:
        return QSize(0, isSectionStart(index.row()) ? 64 : 36);
    default:
        return {};
    }
}

Qt::ItemFlags AiChatListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }
    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void AiChatListModel::setEntries(QVector<AiChatListEntry> entries)
{
    beginResetModel();
    m_entries = std::move(entries);
    sortEntries();
    endResetModel();
}

AiChatListEntry AiChatListModel::entryAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }
    return m_entries.at(index.row());
}

int AiChatListModel::rowOfConversation(const QString& conversationId) const
{
    if (conversationId.isEmpty()) {
        return -1;
    }

    for (int row = 0; row < m_entries.size(); ++row) {
        if (m_entries.at(row).conversationId == conversationId) {
            return row;
        }
    }
    return -1;
}

bool AiChatListModel::isSectionStart(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return false;
    }

    if (row == 0) {
        return true;
    }

    return m_entries.at(row - 1).time.date() != m_entries.at(row).time.date();
}

QString AiChatListModel::sectionTitleAt(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return {};
    }
    return sectionTitleForTime(m_entries.at(row).time);
}

int AiChatListModel::sectionStartRowForRow(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return -1;
    }

    for (int current = row; current >= 0; --current) {
        if (isSectionStart(current)) {
            return current;
        }
    }
    return -1;
}

int AiChatListModel::nextSectionStartRow(int row) const
{
    if (row < 0 || row >= m_entries.size()) {
        return -1;
    }

    for (int current = row + 1; current < m_entries.size(); ++current) {
        if (isSectionStart(current)) {
            return current;
        }
    }
    return -1;
}

QString AiChatListModel::sectionTitleForTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }

    const QDate currentDate = QDate::currentDate();
    const QDate itemDate = time.date();
    if (itemDate == currentDate) {
        return QStringLiteral("今天");
    }
    if (itemDate.addDays(1) == currentDate) {
        return QStringLiteral("昨天");
    }
    if (itemDate.year() == currentDate.year()) {
        return time.toString(QStringLiteral("MM月dd日"));
    }
    return time.toString(QStringLiteral("yyyy年MM月dd日"));
}

void AiChatListModel::sortEntries()
{
    std::sort(m_entries.begin(), m_entries.end(),
              [](const AiChatListEntry& lhs, const AiChatListEntry& rhs) {
                  return lhs.time > rhs.time;
              });
}
