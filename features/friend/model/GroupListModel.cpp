#include "GroupListModel.h"

#include <QSize>

namespace {

constexpr int kCategoryHeaderHeight = 36;
constexpr int kGroupItemHeight = 72;

QString normalizedCategoryId(const Group& group)
{
    return group.listGroupId.isEmpty() ? QStringLiteral("gg_joined") : group.listGroupId;
}

QString normalizedCategoryName(const Group& group)
{
    return group.listGroupName.isEmpty() ? QStringLiteral("我加入的群聊") : group.listGroupName;
}

QString displayNameForGroup(const Group& group)
{
    return group.remark.isEmpty() ? group.groupName : group.remark;
}

} // namespace

GroupListModel::GroupListModel(QObject* parent)
    : QAbstractListModel(parent)
{
}

int GroupListModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant GroupListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const RowEntry& row = m_rows.at(index.row());
    if (row.type == RowEntry::Notice) {
        switch (role) {
        case Qt::DisplayRole:
        case DisplayNameRole:
            return QStringLiteral("群通知");
        case IsNoticeRole:
            return true;
        case IsCategoryRole:
            return false;
        case Qt::SizeHintRole:
            return QSize(0, kCategoryHeaderHeight);
        default:
            return {};
        }
    }

    if (row.categoryIndex < 0 || row.categoryIndex >= m_categories.size()) {
        return {};
    }

    const GroupCategory& category = m_categories.at(row.categoryIndex);
    if (row.type == RowEntry::Category) {
        switch (role) {
        case Qt::DisplayRole:
        case DisplayNameRole:
        case CategoryNameRole:
            return category.categoryName;
        case IsCategoryRole:
            return true;
        case IsNoticeRole:
            return false;
        case CategoryIdRole:
            return category.categoryId;
        case CategoryGroupCountRole:
            return category.groups.size();
        case CategoryExpandedRole:
            return category.expanded;
        case CategoryProgressRole:
            return category.progress;
        case Qt::SizeHintRole:
            return QSize(0, kCategoryHeaderHeight);
        default:
            return {};
        }
    }

    if (row.groupIndex < 0 || row.groupIndex >= category.groups.size()) {
        return {};
    }

    const Group& group = category.groups.at(row.groupIndex);
    switch (role) {
    case Qt::DisplayRole:
    case DisplayNameRole:
        return displayNameForGroup(group);
    case GroupIdRole:
        return group.groupId;
    case AvatarPathRole:
        return group.groupAvatarPath;
    case MemberCountRole:
        return group.memberNum;
    case DoNotDisturbRole:
        return group.isDnd;
    case GroupNameRole:
        return group.groupName;
    case RemarkRole:
        return group.remark;
    case IsCategoryRole:
        return false;
    case IsNoticeRole:
        return false;
    case CategoryIdRole:
        return category.categoryId;
    case CategoryNameRole:
        return category.categoryName;
    case CategoryProgressRole:
        return category.progress;
    case Qt::SizeHintRole:
        return QSize(0, qRound(kGroupItemHeight * category.progress));
    default:
        return {};
    }
}

Qt::ItemFlags GroupListModel::flags(const QModelIndex& index) const
{
    if (!index.isValid()) {
        return Qt::NoItemFlags;
    }

    if (isNoticeRow(index) || isCategoryRow(index)) {
        return Qt::ItemIsEnabled;
    }

    if (index.data(CategoryProgressRole).toReal() <= 0.01) {
        return Qt::NoItemFlags;
    }

    return Qt::ItemIsEnabled | Qt::ItemIsSelectable;
}

void GroupListModel::setGroups(QVector<Group> groups)
{
    QHash<QString, bool> expandedByCategory;
    QHash<QString, qreal> progressByCategory;
    for (const GroupCategory& category : m_categories) {
        expandedByCategory.insert(category.categoryId, category.expanded);
        progressByCategory.insert(category.categoryId, category.progress);
    }

    beginResetModel();
    m_categories.clear();
    QHash<QString, int> categoryRows;
    for (const Group& group : groups) {
        const QString categoryId = normalizedCategoryId(group);
        const QString categoryName = normalizedCategoryName(group);
        int categoryIndex = categoryRows.value(categoryId, -1);
        if (categoryIndex < 0) {
            GroupCategory category;
            category.categoryId = categoryId;
            category.categoryName = categoryName;
            category.expanded = expandedByCategory.value(categoryId, false);
            category.progress = progressByCategory.value(categoryId, category.expanded ? 1.0 : 0.0);
            m_categories.push_back(category);
            categoryIndex = m_categories.size() - 1;
            categoryRows.insert(categoryId, categoryIndex);
        }

        Group normalized = group;
        normalized.listGroupId = categoryId;
        normalized.listGroupName = categoryName;
        m_categories[categoryIndex].groups.push_back(normalized);
    }

    rebuildRows();
    endResetModel();
}

QString GroupListModel::groupIdAt(const QModelIndex& index) const
{
    if (!isGroupRow(index)) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_categories.at(row.categoryIndex).groups.at(row.groupIndex).groupId;
}

QString GroupListModel::categoryIdAt(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    if (row.categoryIndex < 0 || row.categoryIndex >= m_categories.size()) {
        return {};
    }
    return m_categories.at(row.categoryIndex).categoryId;
}

QString GroupListModel::categoryIdForGroup(const QString& groupId) const
{
    for (const GroupCategory& category : m_categories) {
        for (const Group& group : category.groups) {
            if (group.groupId == groupId) {
                return category.categoryId;
            }
        }
    }
    return {};
}

Group GroupListModel::groupAt(const QModelIndex& index) const
{
    if (!isGroupRow(index)) {
        return {};
    }
    const RowEntry& row = m_rows.at(index.row());
    return m_categories.at(row.categoryIndex).groups.at(row.groupIndex);
}

int GroupListModel::indexOfGroup(const QString& groupId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const RowEntry& entry = m_rows.at(row);
        if (entry.type != RowEntry::GroupChat ||
            entry.categoryIndex < 0 ||
            entry.categoryIndex >= m_categories.size()) {
            continue;
        }

        const GroupCategory& category = m_categories.at(entry.categoryIndex);
        if (entry.groupIndex >= 0 &&
            entry.groupIndex < category.groups.size() &&
            category.groups.at(entry.groupIndex).groupId == groupId) {
            return row;
        }
    }
    return -1;
}

int GroupListModel::categoryRowForId(const QString& categoryId) const
{
    return rowForCategory(categoryId);
}

int GroupListModel::categoryRowForRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    for (int current = row; current >= 0; --current) {
        if (m_rows.at(current).type == RowEntry::Category) {
            return current;
        }
    }
    return -1;
}

int GroupListModel::nextCategoryRow(int row) const
{
    if (row < 0 || row >= m_rows.size()) {
        return -1;
    }

    for (int current = row + 1; current < m_rows.size(); ++current) {
        if (m_rows.at(current).type == RowEntry::Category) {
            return current;
        }
    }
    return -1;
}

bool GroupListModel::isCategoryRow(const QModelIndex& index) const
{
    return index.isValid() &&
           index.row() >= 0 &&
           index.row() < m_rows.size() &&
           m_rows.at(index.row()).type == RowEntry::Category;
}

bool GroupListModel::isGroupRow(const QModelIndex& index) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return false;
    }

    const RowEntry& row = m_rows.at(index.row());
    return row.type == RowEntry::GroupChat &&
           row.categoryIndex >= 0 &&
           row.categoryIndex < m_categories.size() &&
           row.groupIndex >= 0 &&
           row.groupIndex < m_categories.at(row.categoryIndex).groups.size();
}

bool GroupListModel::isNoticeRow(const QModelIndex& index) const
{
    return index.isValid() &&
           index.row() >= 0 &&
           index.row() < m_rows.size() &&
           m_rows.at(index.row()).type == RowEntry::Notice;
}

bool GroupListModel::isCategoryExpanded(const QString& categoryId) const
{
    const GroupCategory* category = categoryForId(categoryId);
    return category ? category->expanded : false;
}

qreal GroupListModel::categoryProgress(const QString& categoryId) const
{
    const GroupCategory* category = categoryForId(categoryId);
    return category ? category->progress : 0.0;
}

void GroupListModel::setCategoryExpanded(const QString& categoryId, bool expanded)
{
    GroupCategory* category = categoryForId(categoryId);
    if (!category || category->expanded == expanded) {
        return;
    }

    category->expanded = expanded;
    const int categoryRow = rowForCategory(categoryId);
    if (categoryRow >= 0) {
        const QModelIndex changed = index(categoryRow, 0);
        emit dataChanged(changed, changed, {CategoryExpandedRole});
    }
}

void GroupListModel::setCategoryProgress(const QString& categoryId, qreal progress)
{
    GroupCategory* category = categoryForId(categoryId);
    if (!category) {
        return;
    }

    const qreal boundedProgress = qBound(0.0, progress, 1.0);
    if (qFuzzyCompare(category->progress, boundedProgress)) {
        return;
    }

    category->progress = boundedProgress;
    const int firstRow = rowForCategory(categoryId);
    const int lastRow = lastRowForCategory(categoryId);
    if (firstRow >= 0 && lastRow >= firstRow) {
        emit dataChanged(index(firstRow, 0),
                         index(lastRow, 0),
                         {CategoryProgressRole, Qt::SizeHintRole});
    }
}

const GroupListModel::GroupCategory* GroupListModel::categoryForId(const QString& categoryId) const
{
    for (const GroupCategory& category : m_categories) {
        if (category.categoryId == categoryId) {
            return &category;
        }
    }
    return nullptr;
}

GroupListModel::GroupCategory* GroupListModel::categoryForId(const QString& categoryId)
{
    for (GroupCategory& category : m_categories) {
        if (category.categoryId == categoryId) {
            return &category;
        }
    }
    return nullptr;
}

void GroupListModel::rebuildRows()
{
    m_rows.clear();
    m_rows.push_back(RowEntry{RowEntry::Notice, -1, -1});
    for (int categoryIndex = 0; categoryIndex < m_categories.size(); ++categoryIndex) {
        m_rows.push_back(RowEntry{RowEntry::Category, categoryIndex, -1});
        const GroupCategory& category = m_categories.at(categoryIndex);
        for (int groupIndex = 0; groupIndex < category.groups.size(); ++groupIndex) {
            m_rows.push_back(RowEntry{RowEntry::GroupChat, categoryIndex, groupIndex});
        }
    }
}

int GroupListModel::rowForCategory(const QString& categoryId) const
{
    for (int row = 0; row < m_rows.size(); ++row) {
        const RowEntry& entry = m_rows.at(row);
        if (entry.type == RowEntry::Category &&
            entry.categoryIndex >= 0 &&
            entry.categoryIndex < m_categories.size() &&
            m_categories.at(entry.categoryIndex).categoryId == categoryId) {
            return row;
        }
    }
    return -1;
}

int GroupListModel::lastRowForCategory(const QString& categoryId) const
{
    const int categoryRow = rowForCategory(categoryId);
    if (categoryRow < 0) {
        return -1;
    }

    for (int row = categoryRow + 1; row < m_rows.size(); ++row) {
        if (m_rows.at(row).type == RowEntry::Category) {
            return row - 1;
        }
    }
    return m_rows.size() - 1;
}
