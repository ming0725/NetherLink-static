#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>

class UnreadStateRepository : public QObject {
    Q_OBJECT
    Q_DISABLE_COPY(UnreadStateRepository)

public:
    static UnreadStateRepository& instance();

    bool isUnread(const QString& scope, const QString& itemId) const;
    int unreadCount(const QString& scope) const;
    void setUnread(const QString& scope, const QString& itemId, bool unread);
    void markAllRead(const QString& scope);

signals:
    void unreadCountChanged(const QString& scope, int count);

private:
    explicit UnreadStateRepository(QObject* parent = nullptr);

    QHash<QString, QSet<QString>> m_unreadByScope;
};
