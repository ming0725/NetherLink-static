#pragma once

#include <QAbstractListModel>
#include <QDateTime>
#include <QString>
#include <QVector>

struct AiChatListEntry {
    QString title;
    QDateTime time;
};

class AiChatListModel : public QAbstractListModel
{
public:
    enum Role {
        TitleRole = Qt::UserRole + 1,
        TimeRole,
        SectionTitleRole,
        IsSectionStartRole
    };

    explicit AiChatListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void addEntry(AiChatListEntry entry);
    void updateTitle(int row, const QString& title);
    void removeRowAt(int row);

    AiChatListEntry entryAt(const QModelIndex& index) const;
    bool isSectionStart(int row) const;
    QString sectionTitleAt(int row) const;
    int sectionStartRowForRow(int row) const;
    int nextSectionStartRow(int row) const;

private:
    static QString sectionTitleForTime(const QDateTime& time);
    void sortEntries();

    QVector<AiChatListEntry> m_entries;
};
