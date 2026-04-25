#pragma once

#include <QStyledItemDelegate>

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

private:
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
};
