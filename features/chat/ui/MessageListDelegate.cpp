#include "MessageListDelegate.h"

#include <QApplication>
#include <QPainter>

#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/BadgeRenderer.h"
#include "features/chat/model/MessageListModel.h"

namespace {

const QFont& timeFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(11);
        return value;
    }();
    return font;
}

const QFontMetrics& timeMetrics()
{
    static const QFontMetrics metrics(timeFont());
    return metrics;
}

const QFont& nameFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(14);
        return value;
    }();
    return font;
}

const QFontMetrics& nameMetrics()
{
    static const QFontMetrics metrics(nameFont());
    return metrics;
}

const QFont& previewFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(13);
        return value;
    }();
    return font;
}

const QFontMetrics& previewMetrics()
{
    static const QFontMetrics metrics(previewFont());
    return metrics;
}

int fullMonthsBetween(const QDate& from, const QDate& to)
{
    int months = (to.year() - from.year()) * 12 + (to.month() - from.month());
    if (to.day() < from.day()) {
        --months;
    }
    return months;
}

int fullYearsBetween(const QDate& from, const QDate& to)
{
    int years = to.year() - from.year();
    if (to.month() < from.month() ||
        (to.month() == from.month() && to.day() < from.day())) {
        --years;
    }
    return years;
}

QString formatConversationTime(const QDateTime& timestamp)
{
    if (!timestamp.isValid()) {
        return {};
    }

    const QDateTime now = QDateTime::currentDateTime();
    if (timestamp > now) {
        return timestamp.toString(QStringLiteral("HH:mm"));
    }

    const QString minuteText = timestamp.toString(QStringLiteral("HH:mm"));
    const int dayDiff = timestamp.date().daysTo(now.date());
    if (dayDiff <= 0) {
        return minuteText;
    }
    if (dayDiff == 1) {
        return QStringLiteral("昨天 %1").arg(minuteText);
    }
    if (dayDiff < 30) {
        return QStringLiteral("%1天前").arg(dayDiff);
    }

    const int monthDiff = fullMonthsBetween(timestamp.date(), now.date());
    if (monthDiff > 0 && monthDiff < 12) {
        return QStringLiteral("%1个月前").arg(monthDiff);
    }

    const int yearDiff = fullYearsBetween(timestamp.date(), now.date());
    if (yearDiff > 0) {
        return QStringLiteral("%1年前").arg(yearDiff);
    }

    return QStringLiteral("%1天前").arg(dayDiff);
}

} // namespace

MessageListDelegate::MessageListDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void MessageListDelegate::paint(QPainter* painter,
                                const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    const bool selected = option.state & QStyle::State_Selected;
    const bool hovered = (option.state & QStyle::State_MouseOver) ||
            index.data(MessageListModel::ContextMenuActiveRole).toBool();
    const bool pinned = index.data(MessageListModel::IsPinnedRole).toBool();
    const QColor backgroundColor = selected
            ? ThemeManager::instance().color(ThemeColor::ListSelected)
            : (pinned
               ? (hovered ? ThemeManager::instance().color(ThemeColor::ListHover)
                          : ThemeManager::instance().color(ThemeColor::ListPinned))
               : (hovered ? ThemeManager::instance().color(ThemeColor::ListHover)
                          : ThemeManager::instance().color(ThemeColor::PanelBackground)));
    painter->fillRect(option.rect, backgroundColor);
    const QColor selectedTextColor = ThemeManager::textColorOn(backgroundColor);

    const int itemCenterY = option.rect.center().y();
    const QRect avatarRect(option.rect.left() + kLeftPadding,
                           option.rect.top() + (kItemHeight - kAvatarSize) / 2,
                           kAvatarSize,
                           kAvatarSize);
    const qreal devicePixelRatio = painter->device()->devicePixelRatioF();
    const QString avatarPath = index.data(MessageListModel::AvatarPathRole).toString();
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   kAvatarSize,
                                                                   devicePixelRatio);
    painter->drawPixmap(avatarRect, avatar);

    const int contentLeft = avatarRect.right() + kContentSpacing + 1;
    const int rightEdge = option.rect.right() - kRightPadding;

    const QDateTime lastTime = index.data(MessageListModel::LastTimeRole).toDateTime();
    const QString timeText = formatConversationTime(lastTime);
    const int timeWidth = timeText.isEmpty() ? 0 : timeMetrics().horizontalAdvance(timeText);
    const int timeHeight = timeMetrics().height();
    const int nameHeight = nameMetrics().height();
    const int nameY = itemCenterY - kLineSpacing / 2 - nameHeight;

    const QRect timeRect(rightEdge - timeWidth,
                         nameY + (nameHeight - timeHeight) / 2,
                         timeWidth,
                         timeHeight);

    const int unreadCount = index.data(MessageListModel::UnreadCountRole).toInt();
    const bool doNotDisturb = index.data(MessageListModel::DoNotDisturbRole).toBool();
    BadgeLayout badgeLayout = BadgeRenderer::layoutForUnreadCount(
        unreadCount, doNotDisturb, selected, ThemeManager::instance().isDark());
    badgeLayout.invertIcon = selected
            && badgeLayout.drawIcon
            && selectedTextColor == QColor(Qt::black);
    const int badgeX = rightEdge - badgeLayout.size.width();

    const int previewHeight = previewMetrics().height();
    const int previewY = itemCenterY + kLineSpacing / 2;

    const int previewRight = badgeLayout.size.isValid()
            ? badgeX - kBadgeGap
            : rightEdge;
    const QRect previewRect(contentLeft,
                            previewY,
                            qMax(0, previewRight - contentLeft),
                            previewHeight);

    const int nameRight = timeText.isEmpty()
            ? rightEdge
            : timeRect.left() - kTimeGap;
    const QRect nameRect(contentLeft,
                         nameY,
                         qMax(0, nameRight - contentLeft),
                         nameHeight);

    painter->setFont(nameFont());
    painter->setPen(selected ? selectedTextColor : ThemeManager::instance().color(ThemeColor::PrimaryText));
    painter->drawText(nameRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      nameMetrics().elidedText(index.data(MessageListModel::TitleRole).toString(),
                                               Qt::ElideRight,
                                               nameRect.width()));

    if (!timeText.isEmpty()) {
        painter->setFont(timeFont());
        painter->setPen(selected ? selectedTextColor : ThemeManager::instance().color(ThemeColor::TertiaryText));
        painter->drawText(timeRect, Qt::AlignLeft | Qt::AlignVCenter, timeText);
    }

    painter->setFont(previewFont());
    painter->setPen(selected ? selectedTextColor : ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(previewRect,
                      Qt::AlignLeft | Qt::AlignVCenter,
                      previewMetrics().elidedText(index.data(MessageListModel::PreviewTextRole).toString(),
                                                  Qt::ElideRight,
                                                  previewRect.width()));

    if (badgeLayout.size.isValid()) {
        const QRect badgeRect(badgeX,
                              previewY + (previewHeight - badgeLayout.size.height()) / 2,
                              badgeLayout.size.width(),
                              badgeLayout.size.height());
        BadgeRenderer::drawBadge(painter, badgeRect, badgeLayout, selected);
    }

    painter->restore();
}

QSize MessageListDelegate::sizeHint(const QStyleOptionViewItem& option,
                                    const QModelIndex& index) const
{
    Q_UNUSED(option);
    Q_UNUSED(index);
    return QSize(0, kItemHeight);
}
