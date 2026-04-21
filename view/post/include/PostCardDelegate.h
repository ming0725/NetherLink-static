#pragma once

#include <QStyledItemDelegate>

class PostCardDelegate : public QStyledItemDelegate
{
    Q_OBJECT

public:
    enum Action {
        NoAction,
        OpenPostAction,
        OpenAuthorAction,
        ToggleLikeAction
    };

    explicit PostCardDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    int heightForWidth(const QModelIndex& index, int width, qreal devicePixelRatio = 1.0) const;
    QRect imageRect(const QStyleOptionViewItem& option,
                    const QModelIndex& index) const;
    Action actionAt(const QStyleOptionViewItem& option,
                    const QModelIndex& index,
                    const QPoint& point) const;

private:
    struct CardLayout {
        QRect imageRect;
        QRect titleRect;
        QRect avatarRect;
        QRect authorRect;
        QRect likeIconRect;
        QRect likeCountRect;
        int totalHeight = 0;
    };

    CardLayout calculateLayout(const QModelIndex& index, const QRect& rect) const;
    int imageHeightForWidth(const QModelIndex& index, int width) const;

    static constexpr int kMinWidth = 200;
    static constexpr int kMaxImageHeight = 300;
    static constexpr int kMargin = 6;
    static constexpr int kAvatarSize = 30;
    static constexpr int kLikeIconSize = 20;
    static constexpr int kTitleMaxLines = 2;
    static constexpr int kTitleFontSize = 12;
    static constexpr int kMetaFontSize = 9;
};
