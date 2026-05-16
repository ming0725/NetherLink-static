#pragma once

#include <QColor>
#include <QHash>
#include <QModelIndex>
#include <QStyledItemDelegate>
#include <QString>
#include <QVector>

class FriendListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit FriendListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    void clearPaintCache();
    void invalidatePaintCache(const QModelIndex& topLeft,
                              const QModelIndex& bottomRight,
                              const QVector<int>& roles = {});

private:
    struct ThemePaintCache {
        bool valid = false;
        bool dark = false;
        QColor panelBackground;
        QColor listHover;
        QColor listSelected;
        QColor primaryText;
        QColor secondaryText;
        QColor tertiaryText;
        QColor selectedText;
        QColor selectedSecondaryText;
        QColor imagePlaceholder;
    };

    struct FriendPaintCache {
        QString userId;
        QString displayName;
        QString avatarPath;
        QString nickName;
        QString remark;
        QString subtitle;
        QString statusIconPath;
        int status = 0;
    };

    const ThemePaintCache& themePaintCache() const;
    FriendPaintCache friendPaintCache(const QModelIndex& index) const;

    static constexpr int kItemHeight = 72;
    static constexpr int kGroupHeaderHeight = 36;
    static constexpr int kAvatarSize = 48;
    static constexpr int kLeftPadding = 12;
    static constexpr int kRightPadding = 12;
    static constexpr int kGroupCountRightPadding = 18;
    static constexpr int kGroupArrowSize = 12;
    static constexpr int kContentSpacing = 6;
    static constexpr int kLineSpacing = 6;
    static constexpr int kStatusIconSize = 11;
    static constexpr int kStatusIconTextGap = 4;

    mutable ThemePaintCache m_themePaintCache;
    mutable QHash<QString, FriendPaintCache> m_friendPaintCache;
};
