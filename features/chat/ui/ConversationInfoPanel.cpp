#include "ConversationInfoPanel.h"

#include "app/state/CurrentUser.h"
#include "shared/services/ImageService.h"
#include "shared/types/ChatMessage.h"
#include "shared/ui/IconLineEdit.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/OverlayScrollArea.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/RedstoneLampSwitch.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"

#include <QAction>
#include <QApplication>
#include <QCollator>
#include <QContextMenuEvent>
#include <QEvent>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPaintEvent>
#include <QPalette>
#include <QScrollBar>
#include <QStackedWidget>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>
#include <utility>

namespace {
constexpr int kPanelTopMargin = 16;
constexpr int kPanelHorizontalMargin = 10;
constexpr int kRemarkRowHeight = 36;
constexpr int kSwitchRowHeight = 36;
constexpr int kActionButtonHeight = 36;
constexpr int kSectionSpacing = 18;
constexpr int kPanelScrollBottomPadding = 30;
constexpr int kMemberHeaderHeight = 38;
constexpr int kMemberRowHeight = 42;
constexpr int kMemberAvatarSize = 28;
constexpr int kMemberPreviewLimit = 5;
constexpr int kMemberFullPageSize = 40;
constexpr int kMemberFetchBottomThreshold = 80;
constexpr int kCardBackgroundAlpha = 150;
constexpr int kCardHoverAlpha = 170;
constexpr int kCardPressAlpha = 190;

const QString kPanelBackgroundSource(QStringLiteral(":/resources/icon/options_background.png"));
const QColor kPanelFallbackBackground(0xEF, 0xEF, 0xEF);
const QColor kCardBackground(0, 0, 0, kCardBackgroundAlpha);
const QColor kCardHover(0, 0, 0, kCardHoverAlpha);
const QColor kCardPress(0, 0, 0, kCardPressAlpha);
const QColor kTextColor(Qt::white);

QColor opaqueColor(QColor color)
{
    color.setAlpha(255);
    return color;
}

QColor memberListBackgroundColor()
{
    return opaqueColor(ThemeManager::instance().color(ThemeColor::ChatInfoMemberListBackground));
}

QColor memberListHoverColor()
{
    return opaqueColor(ThemeManager::instance().color(ThemeColor::ChatInfoMemberListHover));
}

QColor memberListPrimaryTextColor()
{
    return ThemeManager::instance().color(ThemeColor::ChatInfoMemberListPrimaryText);
}

QColor memberListSecondaryTextColor()
{
    return ThemeManager::instance().color(ThemeColor::ChatInfoMemberListSecondaryText);
}

QColor dangerTextColor()
{
    return ThemeManager::instance().color(ThemeColor::DangerText);
}

void setOpaqueWidgetBackground(QWidget* widget, const QColor& color)
{
    if (!widget) {
        return;
    }

    const QColor background = opaqueColor(color);
    widget->setAttribute(Qt::WA_TranslucentBackground, false);
    widget->setAttribute(Qt::WA_NoSystemBackground, false);
    widget->setAutoFillBackground(true);

    QPalette palette = widget->palette();
    palette.setColor(QPalette::Window, background);
    palette.setColor(QPalette::Base, background);
    widget->setPalette(palette);
}

void makeTransparentContainer(QWidget* widget)
{
    if (!widget) {
        return;
    }

    widget->setAutoFillBackground(false);
    widget->setAttribute(Qt::WA_TranslucentBackground);
    widget->setAttribute(Qt::WA_NoSystemBackground);
}

class RectPanel : public QWidget
{
public:
    explicit RectPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setPen(Qt::NoPen);
        painter.setBrush(kCardBackground);
        painter.drawRect(rect());
    }
};

class MemberListPanel : public QWidget
{
public:
    explicit MemberListPanel(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setAutoFillBackground(false);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setPen(Qt::NoPen);
        painter.setBrush(memberListBackgroundColor());
        painter.drawRect(rect());
    }
};

class MemberSummaryHeader : public QWidget
{
public:
    explicit MemberSummaryHeader(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(kMemberHeaderHeight);
        setCursor(Qt::PointingHandCursor);
    }

    void setMemberCount(int count)
    {
        m_count = count;
        update();
    }

    std::function<void()> clicked;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont titleFont = QApplication::font();
        titleFont.setPixelSize(14);
        painter.setFont(titleFont);
        painter.setPen(memberListPrimaryTextColor());
        painter.drawText(rect().adjusted(16, 0, -16, 0),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("群聊成员"));

        QFont actionFont = QApplication::font();
        actionFont.setPixelSize(12);
        painter.setFont(actionFont);
        painter.setPen(memberListSecondaryTextColor());
        painter.drawText(rect().adjusted(16, 0, -16, 0),
                         Qt::AlignRight | Qt::AlignVCenter,
                         QStringLiteral("查看%1名成员").arg(m_count));
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && clicked) {
            clicked();
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    int m_count = 0;
};

class MemberListHeader : public QWidget
{
public:
    explicit MemberListHeader(QWidget* parent = nullptr)
        : QWidget(parent)
    {
        setFixedHeight(42);
        setCursor(Qt::PointingHandCursor);
    }

    void setMemberCount(int count)
    {
        m_count = count;
        update();
    }

    std::function<void()> clicked;

protected:
    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont font = QApplication::font();
        font.setPixelSize(15);
        painter.setFont(font);
        const QFontMetrics metrics(font);
        const int textCenterY = rect().center().y();
        const int arrowHalfHeight = 6;
        const int arrowX = 16;
        const QColor foreground = ThemeManager::instance().color(ThemeColor::ChatInfoMemberListHeaderText);

        QPen arrowPen(foreground, 2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPoint(arrowX + 7, textCenterY - arrowHalfHeight),
                         QPoint(arrowX, textCenterY));
        painter.drawLine(QPoint(arrowX, textCenterY),
                         QPoint(arrowX + 7, textCenterY + arrowHalfHeight));

        painter.setPen(foreground);
        painter.drawText(QRect(34, 0, width() - 44, height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QStringLiteral("群聊人数 %1").arg(m_count));
        Q_UNUSED(metrics);
    }

    void mouseReleaseEvent(QMouseEvent* event) override
    {
        if (event->button() == Qt::LeftButton && rect().contains(event->pos()) && clicked) {
            clicked();
        }
        QWidget::mouseReleaseEvent(event);
    }

private:
    int m_count = 0;
};

QString memberNickname(const Group& group, const QString& userId)
{
    const QString storedNickname = group.memberNicknames.value(userId).trimmed();
    if (!storedNickname.isEmpty()) {
        return storedNickname;
    }
    if (userId == CurrentUser::instance().getUserId() && !group.currentUserNickname.trimmed().isEmpty()) {
        return group.currentUserNickname.trimmed();
    }
    return {};
}

QString memberDisplayName(const Group& group, const User& user)
{
    const QString groupNickname = memberNickname(group, user.id);
    if (!groupNickname.isEmpty()) {
        return groupNickname;
    }
    if (!user.remark.trimmed().isEmpty()) {
        return user.remark.trimmed();
    }
    return user.nick.trimmed().isEmpty() ? user.id : user.nick.trimmed();
}

GroupRole memberRole(const Group& group, const QString& userId)
{
    if (!group.ownerId.isEmpty() && group.ownerId == userId) {
        return GroupRole::Owner;
    }
    if (group.adminsID.contains(userId)) {
        return GroupRole::Admin;
    }
    return GroupRole::Member;
}

GroupRole memberRole(const Group& group, const User& user)
{
    return memberRole(group, user.id);
}

QString roleText(GroupRole role)
{
    if (role == GroupRole::Owner) {
        return QStringLiteral("群主");
    }
    if (role == GroupRole::Admin) {
        return QStringLiteral("管理员");
    }
    return QStringLiteral("用户");
}

QColor roleBackgroundColor(GroupRole role)
{
    if (role == GroupRole::Owner) {
        return QColor(0xF5, 0xDD, 0xCB);
    }
    if (role == GroupRole::Admin) {
        return QColor(0xC2, 0xE1, 0xF5);
    }
    return QColor(0xE8, 0xE8, 0xE8);
}

QColor roleTextColor(GroupRole role)
{
    if (role == GroupRole::Owner) {
        return QColor(0xFF, 0x9C, 0x00);
    }
    if (role == GroupRole::Admin) {
        return QColor(0x00, 0x66, 0xCC);
    }
    return QColor(0x66, 0x66, 0x66);
}

QVector<User> sortedGroupMembers(const Group& group,
                                 const QVector<User>& members,
                                 const QString& keyword = {})
{
    QVector<User> filtered;
    filtered.reserve(members.size());
    const QString normalizedKeyword = keyword.trimmed();
    for (const User& user : members) {
        const QString displayName = memberDisplayName(group, user);
        if (!normalizedKeyword.isEmpty() &&
            !displayName.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.nick.contains(normalizedKeyword, Qt::CaseInsensitive) &&
            !user.remark.contains(normalizedKeyword, Qt::CaseInsensitive)) {
            continue;
        }
        filtered.push_back(user);
    }

    static QCollator collator(QLocale::Chinese);
    collator.setNumericMode(true);
    std::sort(filtered.begin(), filtered.end(), [&group](const User& lhs, const User& rhs) {
        const GroupRole lhsRole = memberRole(group, lhs);
        const GroupRole rhsRole = memberRole(group, rhs);
        const int lhsRank = lhsRole == GroupRole::Owner ? 0 : (lhsRole == GroupRole::Admin ? 1 : 2);
        const int rhsRank = rhsRole == GroupRole::Owner ? 0 : (rhsRole == GroupRole::Admin ? 1 : 2);
        if (lhsRank != rhsRank) {
            return lhsRank < rhsRank;
        }
        const int nameOrder = collator.compare(memberDisplayName(group, lhs), memberDisplayName(group, rhs));
        return nameOrder == 0 ? lhs.id < rhs.id : nameOrder < 0;
    });
    return filtered;
}

class GroupMemberRow : public QWidget
{
public:
    GroupMemberRow(const Group& group, const User& user, QWidget* parent = nullptr)
        : QWidget(parent)
        , m_group(group)
        , m_user(user)
    {
        setFixedHeight(kMemberRowHeight);
        setAutoFillBackground(false);
        setMouseTracking(true);
        setAttribute(Qt::WA_Hover);
    }

    void setContextMenuCallback(std::function<void(const User&, const QPoint&)> callback)
    {
        m_contextMenuCallback = std::move(callback);
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter) {
            m_hovered = true;
            update();
        } else if (event->type() == QEvent::Leave) {
            m_hovered = false;
            update();
        }
        return QWidget::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        QWidget::paintEvent(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::SmoothPixmapTransform);
        painter.setPen(Qt::NoPen);
        painter.setBrush(m_hovered ? memberListHoverColor() : memberListBackgroundColor());
        painter.drawRect(rect());

        const QRect avatarRect(16, (height() - kMemberAvatarSize) / 2, kMemberAvatarSize, kMemberAvatarSize);
        QPainterPath avatarPath;
        avatarPath.addRoundedRect(avatarRect, 4, 4);
        painter.save();
        painter.setClipPath(avatarPath);
        const QPixmap avatar = ImageService::instance().scaled(m_user.avatarPath,
                                                               avatarRect.size(),
                                                               Qt::KeepAspectRatioByExpanding,
                                                               painter.device()->devicePixelRatioF());
        painter.drawPixmap(avatarRect, avatar);
        painter.restore();

        const GroupRole role = memberRole(m_group, m_user);
        int nameRight = width() - 16;
        if (role != GroupRole::Member) {
            const QString badgeText = roleText(role);
            QFont roleFont = QApplication::font();
            roleFont.setPixelSize(11);
            painter.setFont(roleFont);
            const QFontMetrics roleMetrics(roleFont);
            const int badgeWidth = roleMetrics.horizontalAdvance(badgeText) + 12;
            const QRect badgeRect(width() - 16 - badgeWidth,
                                  (height() - 18) / 2,
                                  badgeWidth,
                                  18);
            painter.setBrush(roleBackgroundColor(role));
            painter.setPen(Qt::NoPen);
            painter.drawRoundedRect(badgeRect, 4, 4);
            painter.setPen(roleTextColor(role));
            painter.drawText(badgeRect, Qt::AlignCenter, badgeText);
            nameRight = badgeRect.left() - 12;
        }

        QFont nameFont = QApplication::font();
        nameFont.setPixelSize(13);
        painter.setFont(nameFont);
        painter.setPen(memberListPrimaryTextColor());
        const QFontMetrics nameMetrics(nameFont);
        const int nameLeft = avatarRect.right() + 10;
        const QString name = nameMetrics.elidedText(memberDisplayName(m_group, m_user),
                                                    Qt::ElideRight,
                                                    qMax(0, nameRight - nameLeft));
        painter.drawText(QRect(nameLeft, 0, qMax(0, nameRight - nameLeft), height()),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         name);
    }

    void contextMenuEvent(QContextMenuEvent* event) override
    {
        if (m_contextMenuCallback && !m_user.id.isEmpty()) {
            m_contextMenuCallback(m_user, event->globalPos());
            event->accept();
            return;
        }

        QWidget::contextMenuEvent(event);
    }

private:
    Group m_group;
    User m_user;
    std::function<void(const User&, const QPoint&)> m_contextMenuCallback;
    bool m_hovered = false;
};

QWidget* createOptionLabel(const QString& text, QWidget* parent)
{
    auto* label = new PaintedLabel(text, parent);
    QFont labelFont = label->font();
    labelFont.setPixelSize(14);
    label->setFont(labelFont);
    label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    label->setTextColor(kTextColor);
    return label;
}

void configurePanelEditableText(InlineEditableText* text,
                                const QString& placeholder)
{
    text->setFixedHeight(kRemarkRowHeight);
    text->setPlaceholderText(placeholder);
    text->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    text->setTextColor(kTextColor);
    text->setPlaceholderTextColor(QColor(255, 255, 255, 150));
    text->setNormalBackgroundColor(Qt::transparent);
    text->setHoverBackgroundColor(Qt::transparent);
    text->setFocusBackgroundColor(Qt::transparent);
    text->setNormalBorderColor(Qt::transparent);
    text->setFocusBorderColor(Qt::transparent);
    text->setBorderWidth(0);
    text->setRadius(0);
}

QWidget* createEditableRow(const QString& title,
                           InlineEditableText* control,
                           QWidget* parent)
{
    auto* row = new QWidget(parent);
    row->setFixedHeight(kRemarkRowHeight);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);
    layout->addWidget(createOptionLabel(title, row));
    layout->addWidget(control, 1);
    return row;
}

QWidget* createSwitchRow(const QString& title,
                         RedstoneLampSwitch* control,
                         QWidget* parent)
{
    auto* row = new QWidget(parent);
    row->setFixedHeight(kSwitchRowHeight);

    auto* layout = new QHBoxLayout(row);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);
    layout->addWidget(createOptionLabel(title, row));
    layout->addStretch();
    layout->addWidget(control);
    return row;
}

void applyPanelActionButtonStyle(StatefulPushButton* button,
                                 const QColor& textColor,
                                 Qt::Alignment alignment)
{
    button->setRadius(0);
    button->setNormalColor(kCardBackground);
    button->setHoverColor(kCardHover);
    button->setPressColor(kCardPress);
    button->setTextColor(textColor);
    button->setBorderWidth(0);
    button->setTextAlignment(alignment);
    button->setFixedHeight(kActionButtonHeight);

    QFont font = button->font();
    font.setPixelSize(14);
    button->setFont(font);
}
} // namespace

ConversationInfoPanel::ConversationInfoPanel(QWidget* parent)
    : QWidget(parent)
{
    setAutoFillBackground(false);
    setAttribute(Qt::WA_TranslucentBackground);
    hide();
}

void ConversationInfoPanel::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter painter(this);
    const QPixmap background = ImageService::instance().pixmap(kPanelBackgroundSource);
    if (background.isNull()) {
        painter.fillRect(rect(), kPanelFallbackBackground);
    } else {
        painter.drawTiledPixmap(rect(), background);
    }
    painter.fillRect(rect(), ThemeManager::instance().color(ThemeColor::ChatInfoPanelOverlay));
}

GroupConversationInfoPanel::GroupConversationInfoPanel(QWidget* parent)
    : ConversationInfoPanel(parent)
    , m_groupNameText(new InlineEditableText(this))
    , m_groupIntroductionText(new InlineEditableText(this))
    , m_groupAnnouncementText(new InlineEditableText(this))
    , m_currentUserNicknameText(new InlineEditableText(this))
    , m_groupRemarkText(new InlineEditableText(this))
    , m_pinSwitch(new RedstoneLampSwitch(this))
    , m_doNotDisturbSwitch(new RedstoneLampSwitch(this))
    , m_clearHistoryButton(new StatefulPushButton(QStringLiteral("删除聊天记录"), this))
    , m_exitGroupButton(new StatefulPushButton(QStringLiteral("退出群聊"), this))
    , m_groupInfoCard(new RectPanel(this))
    , m_memberSummaryCard(new MemberListPanel(this))
    , m_memberListPage(new QWidget(this))
    , m_mainScrollArea(new OverlayScrollArea(this))
    , m_memberListScrollArea(new OverlayScrollArea(this))
    , m_stack(new QStackedWidget(this))
{
    auto* rootLayout = new QVBoxLayout(this);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);
    rootLayout->addWidget(m_stack);
    makeTransparentContainer(m_stack);

    auto* layout = m_mainScrollArea->useVerticalContentLayout(
            QMargins(kPanelHorizontalMargin, kPanelTopMargin, kPanelHorizontalMargin, 0),
            0);
    m_mainScrollArea->clearViewportBackground();
    makeTransparentContainer(m_mainScrollArea);
    makeTransparentContainer(m_mainScrollArea->viewport());
    makeTransparentContainer(m_mainScrollArea->getContentWidget());

    auto* groupInfoLayout = new QVBoxLayout(m_groupInfoCard);
    groupInfoLayout->setContentsMargins(0, 0, 0, 0);
    groupInfoLayout->setSpacing(0);
    configurePanelEditableText(m_groupNameText, QStringLiteral("设置群聊名称"));
    configurePanelEditableText(m_groupIntroductionText, QStringLiteral("设置群介绍"));
    configurePanelEditableText(m_groupAnnouncementText, QStringLiteral("设置群公告"));
    groupInfoLayout->addWidget(createEditableRow(QStringLiteral("群聊名称"), m_groupNameText, m_groupInfoCard));
    groupInfoLayout->addWidget(createEditableRow(QStringLiteral("群介绍"), m_groupIntroductionText, m_groupInfoCard));
    groupInfoLayout->addWidget(createEditableRow(QStringLiteral("群公告"), m_groupAnnouncementText, m_groupInfoCard));
    layout->addWidget(m_groupInfoCard);
    layout->addSpacing(kSectionSpacing);

    auto* memberCard = new RectPanel(this);
    auto* memberLayout = new QVBoxLayout(memberCard);
    memberLayout->setContentsMargins(0, 0, 0, 0);
    memberLayout->setSpacing(0);
    configurePanelEditableText(m_currentUserNicknameText, QStringLiteral("设置群昵称"));
    configurePanelEditableText(m_groupRemarkText, QStringLiteral("设置群聊备注"));
    memberLayout->addWidget(createEditableRow(QStringLiteral("我的群聊昵称"), m_currentUserNicknameText, memberCard));
    memberLayout->addWidget(createEditableRow(QStringLiteral("群聊备注"), m_groupRemarkText, memberCard));
    memberLayout->addWidget(createSwitchRow(QStringLiteral("设为置顶"), m_pinSwitch, memberCard));
    memberLayout->addWidget(createSwitchRow(QStringLiteral("免打扰"), m_doNotDisturbSwitch, memberCard));
    layout->addWidget(memberCard);
    layout->addSpacing(kSectionSpacing);

    auto* summaryLayout = new QVBoxLayout(m_memberSummaryCard);
    summaryLayout->setContentsMargins(0, 0, 0, 0);
    summaryLayout->setSpacing(0);
    auto* summaryHeader = new MemberSummaryHeader(m_memberSummaryCard);
    summaryHeader->clicked = [this]() { showMemberListPage(); };
    m_memberSummaryHeader = summaryHeader;
    summaryLayout->addWidget(summaryHeader);
    m_memberPreviewLayout = new QVBoxLayout();
    m_memberPreviewLayout->setContentsMargins(0, 0, 0, 0);
    m_memberPreviewLayout->setSpacing(0);
    summaryLayout->addLayout(m_memberPreviewLayout);
    layout->addWidget(m_memberSummaryCard);
    layout->addSpacing(kSectionSpacing);

    applyPanelActionButtonStyle(m_clearHistoryButton, kTextColor, Qt::AlignLeft | Qt::AlignVCenter);
    applyPanelActionButtonStyle(m_exitGroupButton, dangerTextColor(), Qt::AlignCenter);
    layout->addWidget(m_clearHistoryButton);
    layout->addSpacing(kSectionSpacing);
    layout->addWidget(m_exitGroupButton);
    layout->addSpacing(kPanelScrollBottomPadding);
    m_stack->addWidget(m_mainScrollArea);

    auto* listPageLayout = new QVBoxLayout(m_memberListPage);
    listPageLayout->setContentsMargins(kPanelHorizontalMargin, 8, kPanelHorizontalMargin, 0);
    listPageLayout->setSpacing(10);
    makeTransparentContainer(m_memberListPage);
    auto* listHeader = new MemberListHeader(m_memberListPage);
    listHeader->clicked = [this]() { showMainPage(); };
    m_memberListHeader = listHeader;
    listPageLayout->addWidget(listHeader);
    m_memberSearchInput = new IconLineEdit(m_memberListPage);
    m_memberSearchInput->setFixedHeight(26);
    listPageLayout->addWidget(m_memberSearchInput);

    m_memberFullListLayout = m_memberListScrollArea->useVerticalContentLayout(QMargins(), 0);
    makeTransparentContainer(m_memberListScrollArea);
    makeTransparentContainer(m_memberListScrollArea->getContentWidget());
    applyMemberListTheme();
    m_memberFullListLayout->setContentsMargins(0, 0, 0, 0);
    m_memberFullListLayout->setSpacing(0);
    m_memberFullListLayout->addSpacing(kPanelScrollBottomPadding);
    listPageLayout->addWidget(m_memberListScrollArea, 1);
    m_stack->addWidget(m_memberListPage);
    m_stack->setCurrentIndex(0);

    connect(m_groupNameText, &InlineEditableText::editingFinished, this, [this]() {
        emit groupNameChanged(m_groupNameText->text());
    });
    connect(m_groupIntroductionText, &InlineEditableText::editingFinished, this, [this]() {
        emit groupIntroductionChanged(m_groupIntroductionText->text());
    });
    connect(m_groupAnnouncementText, &InlineEditableText::editingFinished, this, [this]() {
        emit groupAnnouncementChanged(m_groupAnnouncementText->text());
    });
    connect(m_currentUserNicknameText, &InlineEditableText::editingFinished, this, [this]() {
        emit currentUserNicknameChanged(m_currentUserNicknameText->text());
    });
    connect(m_groupRemarkText, &InlineEditableText::editingFinished, this, [this]() {
        emit groupRemarkChanged(m_groupRemarkText->text());
    });
    connect(m_pinSwitch, &RedstoneLampSwitch::redstonePowerChanged,
            this, &GroupConversationInfoPanel::pinChanged);
    connect(m_doNotDisturbSwitch, &RedstoneLampSwitch::redstonePowerChanged,
            this, &GroupConversationInfoPanel::doNotDisturbChanged);
    connect(m_clearHistoryButton, &StatefulPushButton::clicked,
            this, &GroupConversationInfoPanel::clearChatHistoryRequested);
    connect(m_exitGroupButton, &StatefulPushButton::clicked,
            this, &GroupConversationInfoPanel::exitGroupRequested);
    connect(m_memberSearchInput->getLineEdit(), &QLineEdit::textChanged, this, [this](const QString& keyword) {
        m_memberKeyword = keyword.trimmed();
        if (m_stack && m_stack->currentWidget() == m_memberListPage) {
            clearMemberFullList();
            requestNextMemberPage();
        }
    });
    connect(m_memberListScrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        QScrollBar* scrollBar = m_memberListScrollArea ? m_memberListScrollArea->verticalScrollBar() : nullptr;
        if (!scrollBar || !m_memberHasMore || m_memberLoading) {
            return;
        }
        if (scrollBar->maximum() - value <= kMemberFetchBottomThreshold) {
            requestNextMemberPage();
        }
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyMemberListTheme();
    });
}

void GroupConversationInfoPanel::setGroupSummary(const Group& group,
                                                 const ConversationMeta& meta,
                                                 const QVector<User>& previewMembers,
                                                 int totalMembers,
                                                 bool canEditGroupInfo,
                                                 bool canExitGroup)
{
    const bool validGroup = !group.groupId.isEmpty();
    m_group = group;
    m_members = previewMembers;
    m_memberTotalCount = qMax(0, totalMembers);
    m_canEditGroupInfo = validGroup && canEditGroupInfo;
    m_canExitGroup = validGroup && canExitGroup;

    m_groupInfoCard->setVisible(m_canEditGroupInfo);
    m_groupNameText->setText(m_canEditGroupInfo ? group.groupName : QString());
    m_groupIntroductionText->setText(m_canEditGroupInfo ? group.introduction : QString());
    m_groupAnnouncementText->setText(m_canEditGroupInfo ? group.announcement : QString());
    m_currentUserNicknameText->setText(validGroup
                                       ? memberNickname(group, CurrentUser::instance().getUserId())
                                       : QString());
    m_groupRemarkText->setText(validGroup ? group.remark : QString());
    m_pinSwitch->setLampChecked(validGroup && meta.isPinned);
    m_doNotDisturbSwitch->setLampChecked(validGroup && meta.isDoNotDisturb);
    m_exitGroupButton->setEnabled(m_canExitGroup);
    m_memberSummaryCard->setVisible(validGroup);
    rebuildMemberPreview();
    if (validGroup && m_stack && m_stack->currentWidget() == m_memberListPage) {
        clearMemberFullList();
        requestNextMemberPage();
    }
    if (!validGroup) {
        showMainPage();
    }
    m_mainScrollArea->relayoutContent();
}

void GroupConversationInfoPanel::rebuildMemberPreview()
{
    if (!m_memberPreviewLayout || !m_memberSummaryHeader) {
        return;
    }

    static_cast<MemberSummaryHeader*>(m_memberSummaryHeader)->setMemberCount(m_memberTotalCount);
    static_cast<MemberListHeader*>(m_memberListHeader)->setMemberCount(m_memberTotalCount);

    while (QLayoutItem* item = m_memberPreviewLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    const QVector<User> members = sortedGroupMembers(m_group, m_members);
    const int visibleCount = qMin(kMemberPreviewLimit, members.size());
    for (int index = 0; index < visibleCount; ++index) {
        auto* row = new GroupMemberRow(m_group, members.at(index), m_memberSummaryCard);
        row->setContextMenuCallback([this](const User& user, const QPoint& globalPos) {
            showMemberContextMenu(user, globalPos);
        });
        m_memberPreviewLayout->addWidget(row);
    }
    m_mainScrollArea->relayoutContent();
}

void GroupConversationInfoPanel::clearMemberFullList()
{
    if (!m_memberFullListLayout) {
        return;
    }

    while (QLayoutItem* item = m_memberFullListLayout->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }

    m_memberLoadedCount = 0;
    m_memberHasMore = m_memberTotalCount > 0;
    m_memberLoading = false;
    m_memberFullListLayout->addSpacing(kPanelScrollBottomPadding);
    m_memberListScrollArea->relayoutContent();
}

void GroupConversationInfoPanel::applyMemberListTheme()
{
    const QColor background = memberListBackgroundColor();
    if (m_memberListScrollArea) {
        m_memberListScrollArea->setViewportBackgroundColor(background);
        setOpaqueWidgetBackground(m_memberListScrollArea->getContentWidget(), background);
        m_memberListScrollArea->update();
        if (QWidget* viewport = m_memberListScrollArea->viewport()) {
            viewport->update();
        }
    }

    if (m_memberSummaryCard) {
        m_memberSummaryCard->update();
    }
    if (m_memberSummaryHeader) {
        m_memberSummaryHeader->update();
    }
    if (m_memberListHeader) {
        m_memberListHeader->update();
    }
}

void GroupConversationInfoPanel::appendGroupMembersPage(const GroupMembersPage& page)
{
    if (!m_memberFullListLayout || page.groupId != m_group.groupId || page.keyword != m_memberKeyword) {
        return;
    }

    m_memberLoading = false;
    m_memberTotalCount = page.totalCount;
    m_memberHasMore = page.hasMore;
    m_memberLoadedCount = page.offset + page.members.size();
    static_cast<MemberSummaryHeader*>(m_memberSummaryHeader)->setMemberCount(m_memberTotalCount);
    static_cast<MemberListHeader*>(m_memberListHeader)->setMemberCount(m_memberTotalCount);

    if (m_memberFullListLayout->count() > 0) {
        QLayoutItem* item = m_memberFullListLayout->takeAt(m_memberFullListLayout->count() - 1);
        if (item && item->widget()) {
            m_memberFullListLayout->addItem(item);
        } else {
            delete item;
        }
    }

    for (const User& member : page.members) {
        auto* row = new GroupMemberRow(m_group, member, m_memberListPage);
        row->setContextMenuCallback([this](const User& user, const QPoint& globalPos) {
            showMemberContextMenu(user, globalPos);
        });
        m_memberFullListLayout->addWidget(row);
    }
    m_memberFullListLayout->addSpacing(kPanelScrollBottomPadding);
    m_memberListScrollArea->relayoutContent();

    QScrollBar* scrollBar = m_memberListScrollArea ? m_memberListScrollArea->verticalScrollBar() : nullptr;
    if (scrollBar && m_memberHasMore && scrollBar->maximum() <= kMemberFetchBottomThreshold) {
        requestNextMemberPage();
    }
}

void GroupConversationInfoPanel::showMemberContextMenu(const User& user, const QPoint& globalPos)
{
    if (m_group.groupId.isEmpty() || user.id.isEmpty()) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->addAction(QStringLiteral("查看资料"));

    if (canEditMemberNickname(user)) {
        QAction* nicknameAction = menu->addAction(QStringLiteral("修改群昵称"));
        connect(nicknameAction, &QAction::triggered, this, [this, user]() {
            promptMemberNicknameChange(user);
        });
    }

    if (canPromoteMemberToAdmin(user)) {
        QAction* promoteAction = menu->addAction(QStringLiteral("设置为管理员"));
        connect(promoteAction, &QAction::triggered, this, [this, user]() {
            emit groupMemberAdminPromotionRequested(user.id);
        });
    }

    if (canCancelMemberAdmin(user)) {
        QAction* cancelAdminAction = menu->addAction(QStringLiteral("取消管理员"));
        connect(cancelAdminAction, &QAction::triggered, this, [this, user]() {
            emit groupMemberAdminCancellationRequested(user.id);
        });
    }

    if (canRemoveMember(user)) {
        QAction* removeAction = menu->addAction(QStringLiteral("移出本群"));
        StyledActionMenu::setActionColors(removeAction,
                                          dangerTextColor(),
                                          QColor(Qt::white),
                                          dangerTextColor());
        connect(removeAction, &QAction::triggered, this, [this, user]() {
            confirmMemberRemoval(user);
        });
    }

    connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
    menu->popupWhenMouseReleased(globalPos);
}

void GroupConversationInfoPanel::promptMemberNicknameChange(const User& user)
{
    if (!canEditMemberNickname(user)) {
        return;
    }

    bool accepted = false;
    const QString currentNickname = memberNickname(m_group, user.id);
    const QString fallbackName = user.nick.trimmed().isEmpty() ? user.id : user.nick.trimmed();
    const QString initialText = currentNickname.isEmpty() ? fallbackName : currentNickname;
    const QString nextNickname = QInputDialog::getText(this,
                                                       QStringLiteral("修改群昵称"),
                                                       QStringLiteral("群昵称"),
                                                       QLineEdit::Normal,
                                                       initialText,
                                                       &accepted).trimmed();
    if (!accepted) {
        return;
    }

    emit groupMemberNicknameChanged(user.id, nextNickname);
}

void GroupConversationInfoPanel::confirmMemberRemoval(const User& user)
{
    if (!canRemoveMember(user)) {
        return;
    }

    const QString displayName = memberDisplayName(m_group, user);
    const int result = QMessageBox::question(this,
                                             QStringLiteral("移出本群"),
                                             QStringLiteral("确认将“%1”移出本群吗？").arg(displayName),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    emit groupMemberRemovalRequested(user.id);
}

bool GroupConversationInfoPanel::canEditMemberNickname(const User& user) const
{
    if (m_group.groupId.isEmpty() || user.id.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (user.id == currentUserId) {
        return true;
    }

    const GroupRole currentRole = memberRole(m_group, currentUserId);
    const GroupRole targetRole = memberRole(m_group, user.id);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

bool GroupConversationInfoPanel::canPromoteMemberToAdmin(const User& user) const
{
    if (m_group.groupId.isEmpty() || user.id.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (user.id == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(m_group, currentUserId);
    const GroupRole targetRole = memberRole(m_group, user.id);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Member;
}

bool GroupConversationInfoPanel::canCancelMemberAdmin(const User& user) const
{
    if (m_group.groupId.isEmpty() || user.id.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (user.id == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(m_group, currentUserId);
    const GroupRole targetRole = memberRole(m_group, user.id);
    return currentRole == GroupRole::Owner && targetRole == GroupRole::Admin;
}

bool GroupConversationInfoPanel::canRemoveMember(const User& user) const
{
    if (m_group.groupId.isEmpty() || user.id.isEmpty()) {
        return false;
    }

    const QString currentUserId = CurrentUser::instance().getUserId();
    if (user.id == currentUserId) {
        return false;
    }

    const GroupRole currentRole = memberRole(m_group, currentUserId);
    const GroupRole targetRole = memberRole(m_group, user.id);
    if (currentRole == GroupRole::Owner) {
        return true;
    }
    return currentRole == GroupRole::Admin && targetRole == GroupRole::Member;
}

void GroupConversationInfoPanel::requestNextMemberPage()
{
    if (m_memberLoading || !m_memberHasMore || m_group.groupId.isEmpty()) {
        return;
    }

    m_memberLoading = true;
    emit groupMembersPageRequested(m_memberKeyword, m_memberLoadedCount, kMemberFullPageSize);
}

void GroupConversationInfoPanel::resetTransientState()
{
    showMainPage();
    if (m_memberSearchInput) {
        m_memberSearchInput->clear();
    }
    m_memberKeyword.clear();
    clearMemberFullList();
}

void GroupConversationInfoPanel::releaseTransientResources()
{
    resetTransientState();
    m_members.clear();
    m_memberTotalCount = 0;
    rebuildMemberPreview();
}

void GroupConversationInfoPanel::showMainPage()
{
    if (m_stack) {
        m_stack->setCurrentIndex(0);
    }
}

void GroupConversationInfoPanel::showMemberListPage()
{
    if (!m_stack || m_memberTotalCount <= 0) {
        return;
    }

    if (m_memberSearchInput) {
        m_memberSearchInput->clear();
        m_memberKeyword.clear();
    }
    clearMemberFullList();
    m_stack->setCurrentWidget(m_memberListPage);
    requestNextMemberPage();
}

DirectConversationInfoPanel::DirectConversationInfoPanel(QWidget* parent)
    : ConversationInfoPanel(parent)
    , m_remarkText(new InlineEditableText(this))
    , m_pinSwitch(new RedstoneLampSwitch(this))
    , m_doNotDisturbSwitch(new RedstoneLampSwitch(this))
    , m_clearHistoryButton(new StatefulPushButton(QStringLiteral("删除聊天记录"), this))
    , m_deleteFriendButton(new StatefulPushButton(QStringLiteral("删除好友"), this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(kPanelHorizontalMargin, kPanelTopMargin, kPanelHorizontalMargin, 0);
    layout->setSpacing(0);

    auto* switchCard = new RectPanel(this);
    auto* switchLayout = new QVBoxLayout(switchCard);
    switchLayout->setContentsMargins(0, 0, 0, 0);
    switchLayout->setSpacing(0);
    auto* remarkRow = new QWidget(switchCard);
    remarkRow->setFixedHeight(kRemarkRowHeight);
    auto* remarkLayout = new QHBoxLayout(remarkRow);
    remarkLayout->setContentsMargins(16, 0, 16, 0);
    remarkLayout->setSpacing(12);
    remarkLayout->addWidget(createOptionLabel(QStringLiteral("备注"), remarkRow));
    remarkLayout->addWidget(m_remarkText, 1);
    m_remarkText->setFixedHeight(kRemarkRowHeight);
    m_remarkText->setPlaceholderText(QStringLiteral("设置备注"));
    m_remarkText->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_remarkText->setTextColor(kTextColor);
    m_remarkText->setPlaceholderTextColor(QColor(255, 255, 255, 150));
    m_remarkText->setNormalBackgroundColor(Qt::transparent);
    m_remarkText->setHoverBackgroundColor(Qt::transparent);
    m_remarkText->setFocusBackgroundColor(Qt::transparent);
    m_remarkText->setNormalBorderColor(Qt::transparent);
    m_remarkText->setFocusBorderColor(Qt::transparent);
    m_remarkText->setBorderWidth(0);
    m_remarkText->setRadius(0);
    switchLayout->addWidget(remarkRow);
    switchLayout->addWidget(createSwitchRow(QStringLiteral("设为置顶"), m_pinSwitch, switchCard));
    switchLayout->addWidget(createSwitchRow(QStringLiteral("消息免打扰"), m_doNotDisturbSwitch, switchCard));
    layout->addWidget(switchCard);
    layout->addSpacing(kSectionSpacing);

    applyPanelActionButtonStyle(m_clearHistoryButton, kTextColor, Qt::AlignLeft | Qt::AlignVCenter);
    applyPanelActionButtonStyle(m_deleteFriendButton, dangerTextColor(), Qt::AlignCenter);

    layout->addWidget(m_clearHistoryButton);
    layout->addSpacing(kSectionSpacing);
    layout->addWidget(m_deleteFriendButton);
    layout->addStretch();

    connect(m_remarkText, &InlineEditableText::editingFinished, this, [this]() {
        emit remarkChanged(m_remarkText->text());
    });
    connect(m_pinSwitch, &RedstoneLampSwitch::redstonePowerChanged,
            this, &DirectConversationInfoPanel::pinChanged);
    connect(m_doNotDisturbSwitch, &RedstoneLampSwitch::redstonePowerChanged,
            this, &DirectConversationInfoPanel::doNotDisturbChanged);
    connect(m_clearHistoryButton, &StatefulPushButton::clicked,
            this, &DirectConversationInfoPanel::clearChatHistoryRequested);
    connect(m_deleteFriendButton, &StatefulPushButton::clicked,
            this, &DirectConversationInfoPanel::deleteFriendRequested);
}

void DirectConversationInfoPanel::setConversationMeta(const ConversationMeta& meta,
                                                      const QString& remark,
                                                      bool animated)
{
    const bool showDirectState = !meta.conversationId.isEmpty() && !meta.isGroup;
    m_remarkText->setText(showDirectState ? remark : QString());
    m_pinSwitch->setLampChecked(showDirectState && meta.isPinned, animated);
    m_doNotDisturbSwitch->setLampChecked(showDirectState && meta.isDoNotDisturb, animated);
}
