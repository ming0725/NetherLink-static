#pragma once

#include <QAbstractListModel>
#include <QHash>
#include <QVector>

#include "shared/types/Group.h"
#include "shared/types/RepositoryTypes.h"

class GroupListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Role {
        GroupIdRole = Qt::UserRole + 1,
        DisplayNameRole,
        AvatarPathRole,
        MemberCountRole,
        DoNotDisturbRole,
        GroupNameRole,
        RemarkRole,
        IsNoticeRole,
        IsCategoryRole,
        CategoryIdRole,
        CategoryNameRole,
        CategoryGroupCountRole,
        CategoryExpandedRole,
        CategoryProgressRole,
        HoverSuppressedRole
    };

    explicit GroupListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;

    void setCategories(QVector<GroupCategorySummary> categories, bool preserveLoadedItems = true);
    void appendGroupsToCategory(const QString& categoryId, QVector<Group> groups, bool hasMore);
    int loadedGroupCount(const QString& categoryId) const;
    bool hasMoreGroups(const QString& categoryId) const;
    void pruneCollapsedRows();
    QString groupIdAt(const QModelIndex& index) const;
    QString categoryIdAt(const QModelIndex& index) const;
    QString categoryIdForGroup(const QString& groupId) const;
    Group groupAt(const QModelIndex& index) const;
    int indexOfGroup(const QString& groupId) const;
    int categoryRowForId(const QString& categoryId) const;
    int categoryRowForRow(int row) const;
    int nextCategoryRow(int row) const;
    bool isCategoryRow(const QModelIndex& index) const;
    bool isGroupRow(const QModelIndex& index) const;
    bool isNoticeRow(const QModelIndex& index) const;
    bool isCategoryExpanded(const QString& categoryId) const;
    qreal categoryProgress(const QString& categoryId) const;
    void setCategoryExpanded(const QString& categoryId, bool expanded);
    void setCategoryProgress(const QString& categoryId, qreal progress);
    void setHoverSuppressedGroup(const QString& groupId);

private:
    struct GroupCategory {
        QString categoryId;
        QString categoryName;
        QVector<Group> groups;
        int totalCount = 0;
        bool hasMore = false;
        bool expanded = false;
        qreal progress = 0.0;
    };

    struct RowEntry {
        enum Type {
            Notice,
            Category,
            GroupChat
        };

        Type type = Category;
        int categoryIndex = -1;
        int groupIndex = -1;
    };

    const GroupCategory* categoryForId(const QString& categoryId) const;
    GroupCategory* categoryForId(const QString& categoryId);
    void rebuildRows();
    int rowForCategory(const QString& categoryId) const;
    int lastRowForCategory(const QString& categoryId) const;

    QVector<GroupCategory> m_categories;
    QVector<RowEntry> m_rows;
    QString m_hoverSuppressedGroupId;
};
