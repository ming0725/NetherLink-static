#pragma once

#include <QAbstractListModel>

#include "shared/types/RepositoryTypes.h"

class AiChatListModel : public QAbstractListModel
{
public:
    enum Role {
        ConversationIdRole = Qt::UserRole + 1,
        TitleRole,
        TimeRole,
        SectionTitleRole,
        IsSectionStartRole
    };

    explicit AiChatListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setEntries(QVector<AiChatListEntry> entries);

    AiChatListEntry entryAt(const QModelIndex& index) const;
    int rowOfConversation(const QString& conversationId) const;
    bool isSectionStart(int row) const;
    QString sectionTitleAt(int row) const;
    int sectionStartRowForRow(int row) const;
    int nextSectionStartRow(int row) const;

private:
    static QString sectionTitleForTime(const QDateTime& time);
    void sortEntries();

    QVector<AiChatListEntry> m_entries;
};
