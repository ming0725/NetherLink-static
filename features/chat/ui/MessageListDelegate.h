#pragma once

#include <QStyledItemDelegate>

class MessageListDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit MessageListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

private:
    static constexpr int kItemHeight = 72;
    static constexpr int kAvatarSize = 48;
    static constexpr int kLeftPadding = 12;
    static constexpr int kRightPadding = 12;
    static constexpr int kContentSpacing = 6;
    static constexpr int kLineSpacing = 6;
    static constexpr int kTimeGap = 12;
    static constexpr int kBadgeGap = 8;
    static constexpr int kBadgeHeight = 16;
    static constexpr int kBadgeHorizontalPadding = 4;
};
