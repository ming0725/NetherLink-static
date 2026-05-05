#include "UnreadStateRepository.h"

UnreadStateRepository::UnreadStateRepository(QObject* parent)
    : QObject(parent)
{
}

UnreadStateRepository& UnreadStateRepository::instance()
{
    static UnreadStateRepository repo;
    return repo;
}

bool UnreadStateRepository::isUnread(const QString& scope, const QString& itemId) const
{
    return m_unreadByScope.value(scope).contains(itemId);
}

int UnreadStateRepository::unreadCount(const QString& scope) const
{
    return m_unreadByScope.value(scope).size();
}

void UnreadStateRepository::setUnread(const QString& scope, const QString& itemId, bool unread)
{
    if (scope.isEmpty() || itemId.isEmpty()) {
        return;
    }

    QSet<QString>& items = m_unreadByScope[scope];
    const bool wasUnread = items.contains(itemId);
    if (wasUnread == unread) {
        return;
    }

    if (unread) {
        items.insert(itemId);
    } else {
        items.remove(itemId);
    }
    emit unreadCountChanged(scope, items.size());
}

void UnreadStateRepository::markAllRead(const QString& scope)
{
    auto it = m_unreadByScope.find(scope);
    if (it == m_unreadByScope.end() || it->isEmpty()) {
        return;
    }

    it->clear();
    emit unreadCountChanged(scope, 0);
}
