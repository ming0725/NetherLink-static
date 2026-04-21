#pragma once

#include <QStyledItemDelegate>

class AiChatListDelegate : public QStyledItemDelegate
{
public:
    explicit AiChatListDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    QRect moreButtonRect(const QStyleOptionViewItem& option,
                         const QModelIndex& index) const;
    int stickyHeaderHeight() const;

private:
    QRect itemBodyRect(const QStyleOptionViewItem& option,
                       const QModelIndex& index) const;
    QRect headerRect(const QStyleOptionViewItem& option,
                     const QModelIndex& index) const;
    QRect titleRect(const QStyleOptionViewItem& option,
                    const QModelIndex& index) const;

    static constexpr int kItemHeight = 36;
    static constexpr int kSectionHeaderHeight = 22;
    static constexpr int kSectionGap = 6;
    static constexpr int kHorizontalMargin = 10;
    static constexpr int kCornerRadius = 8;
    static constexpr int kTextLeftPadding = 12;
    static constexpr int kTextRightPadding = 8;
    static constexpr int kMoreButtonSize = 24;
    static constexpr int kMoreIconSize = 16;
};
