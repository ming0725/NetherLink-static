#pragma once

#include <QStyledItemDelegate>

class FriendSessionController;

class GroupNotificationDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    explicit GroupNotificationDelegate(QObject* parent = nullptr);
    void setController(FriendSessionController* controller);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    int buttonAt(const QStyleOptionViewItem& option,
                 const QModelIndex& index,
                 const QPoint& point) const;

    static constexpr int kItemHeight = 104;
    static constexpr int kCardMarginH = 16;
    static constexpr int kCardMarginV = 6;
    static constexpr int kCardPaddingH = 16;
    static constexpr int kCardPaddingV = 16;
    static constexpr int kCardRadius = 8;
    static constexpr int kAvatarSize = 48;
    static constexpr int kAvatarToContentGap = 12;
    static constexpr int kButtonWidth = 54;
    static constexpr int kButtonHeight = 26;
    static constexpr int kButtonRadius = 6;
    static constexpr int kButtonGap = 8;

private:
    FriendSessionController* m_controller = nullptr;
};
