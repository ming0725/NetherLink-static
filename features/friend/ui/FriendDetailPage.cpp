#include "FriendDetailPage.h"

#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include "features/friend/data/UserRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "shared/services/ImageService.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kAvatarSize = 88;
constexpr int kContentMaxWidth = 660;

QLabel* makeTitleLabel(const QString& text)
{
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    label->setStyleSheet(QStringLiteral("color: %1;")
                                 .arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name()));
    return label;
}

QFrame* makeSeparator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(1);
    line->setProperty("themeSeparator", true);
    line->setStyleSheet(QStringLiteral("color: %1; background: %1; border: none;")
                                .arg(ThemeManager::instance().color(ThemeColor::Divider).name()));
    return line;
}

QHBoxLayout* makeInfoRow(const QString& title, QWidget* valueWidget)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(36);

    auto* titleLabel = makeTitleLabel(title);
    titleLabel->setFixedWidth(108);
    row->addWidget(titleLabel, 0, Qt::AlignTop);
    row->addWidget(valueWidget, 1);
    return row;
}

void applyPrimaryButtonStyle(StatefulPushButton* button)
{
    button->setRadius(8);
    button->setPrimaryStyle();
}

void applyDangerOutlineButtonStyle(StatefulPushButton* button)
{
    button->setRadius(8);
    button->setNormalColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    button->setHoverColor(ThemeManager::instance().isDark()
                          ? QColor(0x45, 0x2b, 0x2f)
                          : QColor(0xff, 0xf4, 0xf4));
    button->setPressColor(ThemeManager::instance().isDark()
                          ? QColor(0x54, 0x30, 0x35)
                          : QColor(0xff, 0xe8, 0xe8));
    button->setTextColor(QColor(0xd9, 0x36, 0x36));
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

} // namespace

class FriendDetailPage::AvatarLabel : public QLabel
{
public:
    explicit AvatarLabel(QWidget* parent = nullptr)
        : QLabel(parent)
    {
        setFixedSize(kAvatarSize, kAvatarSize);
    }

    void setAvatarSource(const QString& source)
    {
        m_source = source;
        updatePixmap();
    }

protected:
    void resizeEvent(QResizeEvent* event) override
    {
        QLabel::resizeEvent(event);
        updatePixmap();
    }

private:
    void updatePixmap()
    {
        const qreal dpr = devicePixelRatioF();
        setPixmap(ImageService::instance().circularAvatar(m_source, qMin(width(), height()), dpr));
    }

    QString m_source;
};

class GroupSelectButton : public QToolButton
{
public:
    explicit GroupSelectButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setFixedSize(172, 34);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setPopupMode(QToolButton::DelayedPopup);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

    void setMenuHoverSuppressed(bool suppressed)
    {
        if (m_menuHoverSuppressed == suppressed) {
            return;
        }

        m_menuHoverSuppressed = suppressed;
        update();
    }

protected:
    bool event(QEvent* event) override
    {
        if (event->type() == QEvent::Enter ||
            event->type() == QEvent::MouseButtonPress) {
            setMenuHoverSuppressed(false);
        }
        return QToolButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(ThemeManager::instance().color(ThemeColor::Divider), 1));
        painter.setBrush((underMouse() && !m_menuHoverSuppressed)
                                 ? ThemeManager::instance().color(ThemeColor::ListHover)
                                 : ThemeManager::instance().color(ThemeColor::InputBackground));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

        QFont textFont = font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);
        painter.setPen(ThemeManager::instance().color(ThemeColor::PrimaryText));
        const QRect textRect = rect().adjusted(14, 0, -34, 0);
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(textFont).elidedText(text(), Qt::ElideRight, textRect.width()));

        const int centerX = width() - 19;
        const int centerY = height() / 2 + 1;
        QPen arrowPen(ThemeManager::instance().color(ThemeColor::TertiaryText),
                      1.8,
                      Qt::SolidLine,
                      Qt::RoundCap,
                      Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(centerX - 6.0, centerY - 3.0), QPointF(centerX, centerY + 4.0));
        painter.drawLine(QPointF(centerX, centerY + 4.0), QPointF(centerX + 6.0, centerY - 3.0));
    }

private:
    bool m_menuHoverSuppressed = false;
};

FriendDetailPage::FriendDetailPage(QWidget* parent)
    : QWidget(parent)
    , m_avatarLabel(new AvatarLabel(this))
    , m_nameLabel(new QLabel(this))
    , m_idLabel(new QLabel(this))
    , m_regionRow(new QWidget(this))
    , m_regionLabel(new QLabel(this))
    , m_statusIcon(new QLabel(this))
    , m_statusLabel(new QLabel(this))
    , m_remarkEdit(new InlineEditableText(this))
    , m_groupButton(new GroupSelectButton(this))
    , m_groupMenu(new StyledActionMenu(this))
    , m_signatureLabel(new QLabel(this))
    , m_messageButton(new StatefulPushButton(QStringLiteral("发消息"), this))
    , m_deleteButton(new StatefulPushButton(QStringLiteral("删除好友"), this))
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(48, 80, 48, 44);
    root->setSpacing(0);

    auto* content = new QWidget(this);
    content->setMaximumWidth(kContentMaxWidth);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    root->addWidget(content, 0, Qt::AlignHCenter);
    root->addStretch(1);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(32);
    header->addWidget(m_avatarLabel, 0, Qt::AlignTop);

    auto* identity = new QVBoxLayout;
    identity->setContentsMargins(0, 6, 0, 0);
    identity->setSpacing(6);

    QFont nameFont = m_nameLabel->font();
    nameFont.setPixelSize(20);
    nameFont.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_nameLabel->setStyleSheet(QStringLiteral("color: %1;")
                                       .arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name()));
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont secondaryFont = m_idLabel->font();
    secondaryFont.setPixelSize(13);
    m_idLabel->setFont(secondaryFont);
    m_regionLabel->setFont(secondaryFont);
    m_statusLabel->setFont(secondaryFont);
    m_idLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_regionLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_statusLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_idLabel->setStyleSheet(QStringLiteral("color: %1;")
                                     .arg(ThemeManager::instance().color(ThemeColor::TertiaryText).name()));
    m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_regionLabel->setStyleSheet(QStringLiteral("color: %1;")
                                         .arg(ThemeManager::instance().color(ThemeColor::TertiaryText).name()));
    m_regionLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_statusLabel->setStyleSheet(QStringLiteral("color: %1;")
                                         .arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name()));

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(6);
    m_statusIcon->setFixedSize(14, 14);
    statusRow->addWidget(m_statusIcon);
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    identity->addWidget(m_nameLabel);
    identity->addWidget(m_idLabel);
    identity->addLayout(statusRow);
    identity->addStretch();
    header->addLayout(identity, 1);
    contentLayout->addLayout(header);

    contentLayout->addSpacing(54);
    contentLayout->addWidget(makeSeparator());
    contentLayout->addSpacing(34);

    auto* regionHost = new QWidget(this);
    auto* regionLayout = new QHBoxLayout(regionHost);
    regionLayout->setContentsMargins(0, 0, 0, 0);
    regionLayout->addWidget(m_regionLabel, 1);
    contentLayout->addWidget(m_regionRow);
    m_regionRow->setLayout(makeInfoRow(QStringLiteral("地区"), regionHost));
    m_regionRow->layout()->setContentsMargins(0, 0, 0, 28);
    m_regionRow->hide();

    m_remarkEdit->setPlaceholderText(QStringLiteral("设置好友备注"));
    m_remarkEdit->setFocusPolicy(Qt::ClickFocus);
    m_remarkEdit->setFixedHeight(34);
    m_remarkEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_remarkEdit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    m_remarkEdit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    m_remarkEdit->setNormalBackgroundColor(Qt::transparent);
    m_remarkEdit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_remarkEdit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_remarkEdit->setNormalBorderColor(Qt::transparent);
    m_remarkEdit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    m_remarkEdit->setBorderWidth(1);
    m_remarkEdit->setRadius(5);
    m_remarkEdit->setHorizontalPadding(8);
    connect(m_remarkEdit, &InlineEditableText::editingFinished, this, &FriendDetailPage::saveRemark);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("备注"), m_remarkEdit));
    contentLayout->addSpacing(28);

    auto* groupRowHost = new QWidget(this);
    auto* groupRow = new QHBoxLayout(groupRowHost);
    groupRow->setContentsMargins(0, 0, 0, 0);
    groupRow->addWidget(m_groupButton, 0, Qt::AlignLeft);
    groupRow->addStretch();
    contentLayout->addLayout(makeInfoRow(QStringLiteral("好友分组"), groupRowHost));
    contentLayout->addSpacing(28);

    m_signatureLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_signatureLabel->setStyleSheet(QStringLiteral("color: %1;")
                                            .arg(ThemeManager::instance().color(ThemeColor::PrimaryText).name()));
    QFont signatureFont = m_signatureLabel->font();
    signatureFont.setPixelSize(14);
    m_signatureLabel->setFont(signatureFont);
    m_signatureLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("签名"), m_signatureLabel));

    contentLayout->addSpacing(58);
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addStretch();
    m_messageButton->setFixedSize(118, 38);
    m_deleteButton->setFixedSize(118, 38);
    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_deleteButton);
    buttons->addWidget(m_messageButton);
    buttons->addWidget(m_deleteButton);
    buttons->addStretch();
    contentLayout->addLayout(buttons);

    connect(m_messageButton, &StatefulPushButton::clicked, this, [this]() {
        if (m_hasUser) {
            emit requestMessage(m_user.id);
        }
    });
    connect(m_deleteButton, &StatefulPushButton::clicked, this, [this]() {
        confirmDeleteFriend();
    });

    connect(m_groupButton, &QToolButton::clicked, this, &FriendDetailPage::showGroupMenu);
    connect(m_groupMenu, &QMenu::aboutToHide, this, [this]() {
        m_groupButton->setDown(false);
        static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(true);
        m_groupButton->update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });

    qApp->installEventFilter(this);
    applyTheme();
}

FriendDetailPage::~FriendDetailPage()
{
    qApp->removeEventFilter(this);
}

void FriendDetailPage::setUserId(const QString& userId)
{
    if (userId.isEmpty()) {
        clear();
        return;
    }
    setUser(UserRepository::instance().requestUserDetail({userId}));
}

void FriendDetailPage::clear()
{
    m_user = {};
    m_hasUser = false;
    m_nameLabel->clear();
    m_idLabel->clear();
    m_regionLabel->clear();
    m_regionRow->hide();
    m_statusLabel->clear();
}

void FriendDetailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSignatureText();
}

void FriendDetailPage::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PageBackground));
}

bool FriendDetailPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && m_remarkEdit->isEditing()) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget != m_remarkEdit && !m_remarkEdit->isAncestorOf(widget)) {
            saveRemark();
            m_remarkEdit->finishEditing();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FriendDetailPage::setUser(const User& user)
{
    if (user.id.isEmpty()) {
        clear();
        return;
    }

    m_user = user;
    m_hasUser = true;
    m_remarkEdit->finishEditing();

    m_nameLabel->setText(m_user.nick);
    m_idLabel->setText(QStringLiteral("ID %1").arg(m_user.id));
    m_regionLabel->setText(m_user.region);
    m_regionRow->setVisible(!m_user.region.isEmpty());
    m_statusLabel->setText(statusText(m_user.status));
    updateAvatar();
    updateRemarkText();
    updateGroupButtonText();
    updateSignatureText();
    QTimer::singleShot(0, this, &FriendDetailPage::updateSignatureText);
}

void FriendDetailPage::updateAvatar()
{
    m_avatarLabel->setAvatarSource(m_user.avatarPath);
    const QPixmap statusPixmap = ImageService::instance().scaled(statusIconPath(m_user.status),
                                                                 QSize(14, 14),
                                                                 Qt::KeepAspectRatio,
                                                                 devicePixelRatioF());
    m_statusIcon->setPixmap(statusPixmap);
}

void FriendDetailPage::applyTheme()
{
    const QList<QLabel*> labels = findChildren<QLabel*>();
    for (QLabel* label : labels) {
        const QVariant roleValue = label->property("themeTextRole");
        if (!roleValue.isValid()) {
            continue;
        }
        const auto role = static_cast<ThemeColor>(roleValue.toInt());
        label->setStyleSheet(QStringLiteral("color: %1;")
                                     .arg(ThemeManager::instance().color(role).name()));
    }

    const QList<QFrame*> separators = findChildren<QFrame*>();
    for (QFrame* separator : separators) {
        if (!separator->property("themeSeparator").toBool()) {
            continue;
        }
        const QString divider = ThemeManager::instance().color(ThemeColor::Divider).name();
        separator->setStyleSheet(QStringLiteral("color: %1; background: %1; border: none;").arg(divider));
    }

    m_remarkEdit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    m_remarkEdit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    m_remarkEdit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_remarkEdit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_remarkEdit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));

    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_deleteButton);
    m_groupButton->update();
    update();
}

void FriendDetailPage::updateRemarkText()
{
    if (m_remarkEdit->text() != m_user.remark) {
        m_remarkEdit->setText(m_user.remark);
    }
}

void FriendDetailPage::updateGroupButtonText()
{
    m_groupButton->setText(m_user.friendGroupName);
}

void FriendDetailPage::updateSignatureText()
{
    const QString signature = m_user.signature;
    if (signature.isEmpty()) {
        m_signatureLabel->clear();
        return;
    }

    const int availableWidth = qMax(0, m_signatureLabel->width());
    const QFontMetrics metrics(m_signatureLabel->font());
    m_signatureLabel->setText(metrics.elidedText(signature, Qt::ElideRight, availableWidth));
}

void FriendDetailPage::saveRemark()
{
    if (!m_hasUser) {
        return;
    }

    const QString remark = m_remarkEdit->text().trimmed();
    if (remark == m_user.remark) {
        return;
    }

    m_user.remark = remark;
    UserRepository::instance().saveUser(m_user);
    updateRemarkText();
}

void FriendDetailPage::showGroupMenu()
{
    if (!m_hasUser) {
        return;
    }

    static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(false);
    rebuildGroupMenu();
    m_groupMenu->setFixedWidth(m_groupButton->width());
    m_groupMenu->popup(m_groupButton->mapToGlobal(QPoint(0, m_groupButton->height())));
    m_groupButton->setDown(false);
    static_cast<GroupSelectButton*>(m_groupButton)->setMenuHoverSuppressed(true);
    m_groupButton->update();
}

void FriendDetailPage::rebuildGroupMenu()
{
    QMenu* menu = m_groupMenu;
    if (!menu) {
        return;
    }
    menu->clear();
    const QList<QActionGroup*> oldGroups = menu->findChildren<QActionGroup*>(QString(), Qt::FindDirectChildrenOnly);
    for (QActionGroup* oldGroup : oldGroups) {
        delete oldGroup;
    }

    auto* group = new QActionGroup(menu);
    group->setExclusive(true);

    auto addGroupAction = [this, menu, group](const QString& groupId, const QString& groupName) {
        QAction* action = menu->addAction(groupName);
        action->setCheckable(true);
        action->setChecked(groupId == m_user.friendGroupId);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, groupId, groupName]() {
            changeGroup(groupId, groupName);
        });
    };

    addGroupAction(m_user.friendGroupId, m_user.friendGroupName);
    const QMap<QString, QString> groups = UserRepository::instance().requestFriendGroups();
    for (auto it = groups.constBegin(); it != groups.constEnd(); ++it) {
        if (it.key() == m_user.friendGroupId) {
            continue;
        }
        addGroupAction(it.key(), it.value());
    }

}

void FriendDetailPage::changeGroup(const QString& groupId, const QString& groupName)
{
    if (!m_hasUser || groupId == m_user.friendGroupId) {
        return;
    }

    m_user.friendGroupId = groupId;
    m_user.friendGroupName = groupName;
    UserRepository::instance().saveUser(m_user);
    updateGroupButtonText();
}

void FriendDetailPage::confirmDeleteFriend()
{
    if (!m_hasUser) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("删除好友"),
                                             QStringLiteral("确认删除该好友吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    MessageRepository::instance().removeConversation(m_user.id);
    UserRepository::instance().removeUser(m_user.id);
    clear();
    emit friendDeleted();
}
