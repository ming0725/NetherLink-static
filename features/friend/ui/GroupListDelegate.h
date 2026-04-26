#pragma once

#include <QStyledItemDelegate>

class GroupListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GroupListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    static constexpr int kItemHeight = 72;
    static constexpr int kCategoryHeaderHeight = 36;
    static constexpr int kAvatarSize = 48;
    static constexpr int kLeftPadding = 12;
    static constexpr int kRightPadding = 12;
    static constexpr int kCategoryCountRightPadding = 18;
    static constexpr int kCategoryArrowSize = 12;
    static constexpr int kContentSpacing = 6;
    static constexpr int kLineSpacing = 6;
    static constexpr int kMemberIconSize = 12;
    static constexpr int kMemberIconTextGap = 4;
};
