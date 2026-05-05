#include "FriendNotificationDelegate.h"

#include <QApplication>
#include <QPainter>

#include "features/friend/model/FriendNotificationListModel.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kLineSpacing = 1;
constexpr int kUnifiedFontSize = 12;
constexpr qreal kButtonBorderWidth = 1.5;

const QFont& nickFont()
{
    static const QFont f = []() {
        QFont font = QApplication::font();
        font.setPixelSize(kUnifiedFontSize);
        font.setWeight(QFont::Medium);
        return font;
    }();
    return f;
}

const QFont& textFont()
{
    static const QFont f = []() {
        QFont font = QApplication::font();
        font.setPixelSize(kUnifiedFontSize);
        font.setWeight(QFont::Normal);
        return font;
    }();
    return f;
}

const QFont& buttonFont()
{
    static const QFont f = []() {
        QFont font = QApplication::font();
        font.setPixelSize(12);
        font.setWeight(QFont::Medium);
        return font;
    }();
    return f;
}

const QFontMetrics& nickFm()   { static const QFontMetrics fm(nickFont()); return fm; }
const QFontMetrics& textFm()   { static const QFontMetrics fm(textFont()); return fm; }

QString formatNotificationTime(const QDateTime& time)
{
    if (!time.isValid()) {
        return {};
    }
    return time.date() == QDate::currentDate()
            ? time.toString(QStringLiteral("HH:mm"))
            : time.toString(QStringLiteral("yyyy/MM/dd"));
}

} // namespace

FriendNotificationDelegate::FriendNotificationDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

// --- geometry helpers ---

static QRect cardRect(const QStyleOptionViewItem& option)
{
    return QRect(option.rect.left() + FriendNotificationDelegate::kCardMarginH,
                 option.rect.top() + FriendNotificationDelegate::kCardMarginV,
                 option.rect.width() - 2 * FriendNotificationDelegate::kCardMarginH,
                 option.rect.height() - 2 * FriendNotificationDelegate::kCardMarginV);
}

static QRect avatarRect(const QRect& card)
{
    const int y = card.top() + (card.height() - FriendNotificationDelegate::kAvatarSize) / 2;
    return QRect(card.left() + FriendNotificationDelegate::kCardPaddingH, y,
                 FriendNotificationDelegate::kAvatarSize,
                 FriendNotificationDelegate::kAvatarSize);
}

static QRect contentBounds(const QRect& card)
{
    const int left = card.left() + FriendNotificationDelegate::kCardPaddingH
                     + FriendNotificationDelegate::kAvatarSize
                     + FriendNotificationDelegate::kAvatarToContentGap;
    return QRect(left,
                 card.top() + FriendNotificationDelegate::kCardPaddingV,
                 card.right() - FriendNotificationDelegate::kCardPaddingH - left,
                 card.height() - 2 * FriendNotificationDelegate::kCardPaddingV);
}

static int textLineHeight() { return textFm().height(); }
static int line1Height() { return nickFm().height(); }
static int line2Height()
{
    return qMax(textLineHeight(), FriendNotificationDelegate::kButtonHeight);
}

static int layoutTop(const QRect& content)
{
    const int totalHeight = line1Height() + kLineSpacing + line2Height()
                            + kLineSpacing + textLineHeight();
    return content.top() + qMax(0, (content.height() - totalHeight) / 2);
}

static QRect line1Rect(const QRect& content)
{
    return QRect(content.left(), layoutTop(content), content.width(), line1Height());
}

static QRect line2Rect(const QRect& content)
{
    const int y = line1Rect(content).bottom() + 1 + kLineSpacing;
    return QRect(content.left(), y, content.width(), line2Height());
}

static QRect line3Rect(const QRect& content)
{
    const int y = line2Rect(content).bottom() + 1 + kLineSpacing;
    return QRect(content.left(), y, content.width(), textLineHeight());
}

static QRect acceptBtnRect(const QRect& card, const QRect& content)
{
    const int right = card.right() - FriendNotificationDelegate::kCardPaddingH;
    const QRect l2 = line2Rect(content);
    const int y = l2.top() + (l2.height() - FriendNotificationDelegate::kButtonHeight) / 2;
    return QRect(right - FriendNotificationDelegate::kButtonWidth * 2
                     - FriendNotificationDelegate::kButtonGap,
                 y,
                 FriendNotificationDelegate::kButtonWidth,
                 FriendNotificationDelegate::kButtonHeight);
}

static QRect rejectBtnRect(const QRect& card, const QRect& content)
{
    const int right = card.right() - FriendNotificationDelegate::kCardPaddingH;
    const QRect l2 = line2Rect(content);
    const int y = l2.top() + (l2.height() - FriendNotificationDelegate::kButtonHeight) / 2;
    return QRect(right - FriendNotificationDelegate::kButtonWidth, y,
                 FriendNotificationDelegate::kButtonWidth,
                 FriendNotificationDelegate::kButtonHeight);
}

static QRect actionRect(const QRect& card, const QRect& content)
{
    const QRect accept = acceptBtnRect(card, content);
    const QRect reject = rejectBtnRect(card, content);
    return QRect(accept.left(), accept.top(), reject.right() - accept.left() + 1, accept.height());
}

static int textRightBeforeAction(const QRect& line, const QRect& card, const QRect& content)
{
    return qMin(line.right(), actionRect(card, content).left() - FriendNotificationDelegate::kButtonGap);
}

int FriendNotificationDelegate::buttonAt(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& point) const
{
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        return -1;
    }

    const QRect card = cardRect(option);
    const QRect c = contentBounds(card);
    const auto st = static_cast<NotificationStatus>(
        index.data(FriendNotificationListModel::StatusRole).toInt());
    if (st != NotificationStatus::Pending) return -1;
    if (acceptBtnRect(card, c).contains(point)) return 0;
    if (rejectBtnRect(card, c).contains(point)) return 1;
    return -1;
}

// ==================== paint ====================

void FriendNotificationDelegate::paint(QPainter* painter,
                                        const QStyleOptionViewItem& option,
                                        const QModelIndex& index) const
{
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);

    painter->fillRect(option.rect, ThemeManager::instance().color(ThemeColor::PageBackground));
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        painter->restore();
        return;
    }

    const QRect card = cardRect(option);
    const QRect c = contentBounds(card);
    const QRect avt = avatarRect(card);

    // 2. Card
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
    painter->drawRoundedRect(card, kCardRadius, kCardRadius);

    // 3. Avatar — vertically centered in card
    const qreal dpr = painter->device()->devicePixelRatioF();
    const QString avPath = index.data(FriendNotificationListModel::AvatarPathRole).toString();
    QPixmap av = ImageService::instance().circularAvatarPreview(avPath, kAvatarSize, dpr);
    if (av.isNull()) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
        painter->drawEllipse(avt);
    } else {
        painter->drawPixmap(avt, av);
    }

    const QString nick   = index.data(FriendNotificationListModel::DisplayNameRole).toString();
    const QString date   = formatNotificationTime(
        index.data(FriendNotificationListModel::RequestDateRole).toDateTime());
    const QString msg    = index.data(FriendNotificationListModel::MessageRole).toString();
    const QString src    = index.data(FriendNotificationListModel::SourceTextRole).toString();
    const auto status    = static_cast<NotificationStatus>(
                               index.data(FriendNotificationListModel::StatusRole).toInt());
    const int hovered = index.data(FriendNotificationListModel::HoveredButtonRole).toInt();
    const bool pendingActionsVisible = status == NotificationStatus::Pending
            && hovered != FriendNotificationListModel::kNoHoveredButton;

    // 4. Line 1:  nick(Accent) + " 请求添加为好友 " + date   — all inline, same font size
    {
        const QRect l1 = line1Rect(c);
        const int textRight = pendingActionsVisible ? textRightBeforeAction(l1, card, c) : l1.right();
        const int textWidth = qMax(0, textRight - l1.left() + 1);
        const QString action = QStringLiteral(" 请求添加为好友 ");
        int x = l1.left();

        // Nick
        const int maxNickW = textWidth / 3;
        const QString elidedNick = nickFm().elidedText(nick, Qt::ElideRight, maxNickW);
        const int nickW = nickFm().horizontalAdvance(elidedNick);
        painter->setFont(nickFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::Accent));
        painter->drawText(QRect(x, l1.top(), nickW, l1.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedNick);
        x += nickW;

        // Action text
        const int availForAction = qMax(0, textRight - x + 1);
        const QString elidedAction = textFm().elidedText(action, Qt::ElideRight, availForAction);
        const int actionW = textFm().horizontalAdvance(elidedAction);
        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(x, l1.top(), actionW, l1.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, elidedAction);
        x += actionW;

        // Date — inline right after action text
        const int availForDate = qMax(0, textRight - x + 1);
        if (availForDate > 0) {
            const QString elidedDate = textFm().elidedText(date, Qt::ElideRight, availForDate);
            painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
            painter->drawText(QRect(x, l1.top(), availForDate, l1.height()),
                              Qt::AlignLeft | Qt::AlignVCenter, elidedDate);
        }
    }

    // 5. Line 2:  留言：message
    {
        const QRect l2 = line2Rect(c);
        const QString label = QStringLiteral("留言：");
        const int labelW = textFm().horizontalAdvance(label);

        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l2.left(), l2.top(), labelW, l2.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, label);

        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        const int msgRight = pendingActionsVisible ? textRightBeforeAction(l2, card, c) : l2.right();
        const int msgW = qMax(0, msgRight - (l2.left() + labelW) + 1);
        painter->drawText(QRect(l2.left() + labelW, l2.top(), msgW, l2.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          textFm().elidedText(msg, Qt::ElideRight, msgW));
    }

    // 6. Line 3:  来源：source
    {
        const QRect l3 = line3Rect(c);
        const QString label = QStringLiteral("来源：");
        const int labelW = textFm().horizontalAdvance(label);

        painter->setFont(textFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l3.left(), l3.top(), labelW, l3.height()),
                          Qt::AlignLeft | Qt::AlignVCenter, label);

        const int srcRight = pendingActionsVisible ? textRightBeforeAction(l3, card, c) : l3.right();
        const int srcW = qMax(0, srcRight - (l3.left() + labelW) + 1);
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(QRect(l3.left() + labelW, l3.top(), srcW, l3.height()),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          textFm().elidedText(src, Qt::ElideRight, srcW));
    }

    // 7. Buttons or status text
    if (status == NotificationStatus::Pending && pendingActionsVisible) {
        const QRect accR = acceptBtnRect(card, c);
        const QRect rejR = rejectBtnRect(card, c);

        painter->setFont(buttonFont());

        // Accept
        {
            QColor bg = ThemeManager::instance().color(ThemeColor::Accent);
            if (hovered == 0) bg = ThemeManager::instance().color(ThemeColor::AccentHover);
            painter->setBrush(bg);
            painter->setPen(QPen(bg, kButtonBorderWidth));
            painter->drawRoundedRect(accR, kButtonRadius, kButtonRadius);
            painter->setPen(Qt::white);
            painter->drawText(accR, Qt::AlignCenter, QStringLiteral("同意"));
        }

        // Reject
        {
            const QColor border = ThemeManager::instance().color(ThemeColor::DangerText);
            QColor bg = Qt::transparent;
            if (hovered == 1) bg = ThemeManager::instance().color(ThemeColor::DangerControlHover);
            painter->setBrush(bg);
            painter->setPen(QPen(border, kButtonBorderWidth));
            painter->drawRoundedRect(rejR, kButtonRadius, kButtonRadius);
            painter->setPen(border);
            painter->drawText(rejR, Qt::AlignCenter, QStringLiteral("拒绝"));
        }
    } else if (status == NotificationStatus::Accepted) {
        painter->setFont(buttonFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(actionRect(card, c),
                          Qt::AlignCenter, QStringLiteral("已同意"));
    } else if (status == NotificationStatus::Rejected) {
        painter->setFont(buttonFont());
        painter->setPen(ThemeManager::instance().color(ThemeColor::SecondaryText));
        painter->drawText(actionRect(card, c),
                          Qt::AlignCenter, QStringLiteral("已拒绝"));
    }

    painter->restore();
}

QSize FriendNotificationDelegate::sizeHint(const QStyleOptionViewItem& option,
                                            const QModelIndex& index) const
{
    if (index.data(FriendNotificationListModel::BottomSpaceRole).toBool()) {
        return QSize(option.rect.width() > 0 ? option.rect.width() : 300,
                     FriendNotificationListModel::kBottomSpaceHeight);
    }
    return QSize(option.rect.width() > 0 ? option.rect.width() : 300, kItemHeight);
}
