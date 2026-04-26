#pragma once

#include "shared/types/Group.h"
#include "shared/ui/OverlayScrollListView.h"

#include <QHash>
#include <QPointer>
#include <QString>

class GroupListDelegate;
class GroupListModel;
class QEvent;
class QMouseEvent;
class QPaintEvent;
class QPainter;
class QShowEvent;
class QTimer;
class QVariantAnimation;

class GroupListWidget : public OverlayScrollListView
{
    Q_OBJECT

public:
    explicit GroupListWidget(QWidget* parent = nullptr);
    void ensureInitialized();
    void setKeyword(const QString& keyword);

    QString selectedGroupId() const;
    Group selectedGroup() const;

signals:
    void selectedGroupChanged(const QString& groupId);

protected:
    void leaveEvent(QEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void showEvent(QShowEvent* event) override;

private:
    struct ViewState {
        QString keyword;
        QString loadedKeyword;
        QString selectedGroupId;
        bool initialized = false;
    };

private slots:
    void onCurrentChanged(const QModelIndex& current, const QModelIndex& previous);
    void reloadGroups();

private:
    struct StickyCategoryData {
        QString categoryId;
        QString title;
        int count = 0;
        qreal progress = 0.0;
    };

    struct StickyHeaderState {
        bool visible = false;
        StickyCategoryData category;
        int offsetY = 0;
    };

    void restoreSelection();
    void toggleCategory(const QModelIndex& index);
    void toggleStickyCategoryById(const QString& categoryId);
    void toggleCategoryById(const QString& categoryId, bool keepCategoryAtTop = false);
    void collapseCategoryById(const QString& categoryId, bool keepCategoryAtTop = false);
    void setCategoryExpandedAnimated(const QString& categoryId, bool expanded, bool keepCategoryAtTop = false);
    void loadNextGroupPage(const QString& categoryId);
    void loadMoreForVisibleCategory();
    bool isCategoryPinnedAtTop(const QString& categoryId) const;
    void scrollCategoryToTop(const QString& categoryId);
    void clearCurrentSelection();
    void updateStickyHeader();
    StickyHeaderState calculateStickyHeaderState() const;
    StickyCategoryData stickyCategoryDataForRow(int row) const;
    void drawStickyHeader() const;
    void drawStickyCategory(QPainter* painter,
                            const QRect& rect,
                            const StickyCategoryData& category) const;

    GroupListModel* m_model;
    GroupListDelegate* m_delegate;
    QTimer* m_searchDebounceTimer;
    ViewState m_state;
    QHash<QString, QPointer<QVariantAnimation>> m_categoryAnimations;
    StickyCategoryData m_stickyCategory;
    bool m_stickyVisible = false;
    bool m_preservingSelection = false;
    int m_stickyOffsetY = 0;
};
