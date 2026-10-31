#include "GroupDetailPage.h"
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMap>
#include <QMouseEvent>
#include <QPainter>
#include <QPaintEvent>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

#include "features/friend/ui/FriendSessionController.h"
#include "shared/services/ImageService.h"
#include "shared/ui/ImageViewer.h"
#include "shared/ui/popup/InWindowPopupDialogs.h"
#include "shared/ui/popup/InWindowPopupOverlay.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/CopyIdButton.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/PaintedLabel.h"
#include "shared/ui/StatefulPushButton.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/ThemedSelectButton.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include "shared/theme/ThemeManager.h"

namespace {

constexpr int kAvatarSize = 88;
constexpr int kInfoTitleWidth = 96;
constexpr int kInfoColumnSpacing = 24;
constexpr int kHeaderSpacing = 30;
constexpr int kHeaderContentLeftInset = 64;
constexpr int kIdentityTopInset = 6;
constexpr int kSeparatorTopSpacing = 32;

PaintedLabel* makeTitleLabel(const QString& text)
{
    auto* label = new PaintedLabel(text);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    return label;
}

QHBoxLayout* makeInfoRow(const QString& title, QWidget* valueWidget)
{
    auto* row = new QHBoxLayout;
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(kInfoColumnSpacing);

    auto* titleLabel = makeTitleLabel(title);
    titleLabel->setFixedWidth(kInfoTitleWidth);
    row->addWidget(titleLabel, 0, Qt::AlignTop);
    row->addWidget(valueWidget, 1);
    return row;
}

PaintedLabel* makeValueLabel(QWidget* parent)
{
    auto* label = new PaintedLabel(parent);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    label->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    label->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    label->setWordWrap(false);
    label->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    label->setFixedHeight(QFontMetrics(font).height() + 2);
    return label;
}

QSpacerItem* makeCompressibleSpacing(int height)
{
    return new QSpacerItem(0, height, QSizePolicy::Minimum, QSizePolicy::Preferred);
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
    button->setHoverColor(ThemeManager::instance().color(ThemeColor::DangerControlHover));
    button->setPressColor(ThemeManager::instance().color(ThemeColor::DangerControlPressed));
    button->setTextColor(ThemeManager::instance().color(ThemeColor::DangerText));
    button->setBorderColor(ThemeManager::instance().color(ThemeColor::Divider));
    button->setBorderWidth(1);
}

const QStringList& editableCategoryOrder()
{
    static const QStringList ids = {
            QStringLiteral("gg_joined"),
            QStringLiteral("gg_college"),
            QStringLiteral("gg_work"),
            QStringLiteral("gg_performance")
    };
    return ids;
}

} // namespace

GroupDetailPage::GroupDetailPage(QWidget* parent)
    : QWidget(parent)
    , m_contentWidget(new QWidget(this))
    , m_nameLabel(new PaintedLabel(this))
    , m_idPrefixLabel(new PaintedLabel(QStringLiteral("群号"), this))
    , m_idLabel(new PaintedLabel(this))
    , m_copyIdButton(new CopyIdButton(this))
    , m_remarkEdit(new InlineEditableText(this))
    , m_categoryButton(new ThemedSelectButton(this))
    , m_categoryMenu(new StyledActionMenu(this))
    , m_introLabel(makeValueLabel(this))
    , m_announcementLabel(makeValueLabel(this))
    , m_memberCountLabel(makeValueLabel(this))
    , m_messageButton(new StatefulPushButton(QStringLiteral("发消息"), this))
    , m_exitButton(new StatefulPushButton(QStringLiteral("退出群聊"), this))
{
    connect(&ImageService::instance(), &ImageService::previewReady, this, [this]() {
        update(avatarRect());
    });

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(88, 64, 88, 40);
    root->setSpacing(0);

    m_contentWidget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* contentLayout = new QVBoxLayout(m_contentWidget);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    root->addWidget(m_contentWidget);
    root->addStretch(1);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(kHeaderContentLeftInset, 0, 0, 0);
    header->setSpacing(kHeaderSpacing);
    header->addSpacing(kAvatarSize);

    auto* identity = new QVBoxLayout;
    identity->setContentsMargins(0, 6, 0, 0);
    identity->setSpacing(8);

    QFont nameFont = m_nameLabel->font();
    nameFont.setPixelSize(20);
    nameFont.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::PrimaryText));
    m_nameLabel->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont secondaryFont = m_idLabel->font();
    secondaryFont.setPixelSize(13);
    m_idPrefixLabel->setFont(secondaryFont);
    m_idLabel->setFont(secondaryFont);
    m_idPrefixLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_idLabel->setProperty("themeTextRole", static_cast<int>(ThemeColor::TertiaryText));
    m_idPrefixLabel->setTextColor(ThemeManager::instance().color(ThemeColor::TertiaryText));
    m_idLabel->setTextColor(ThemeManager::instance().color(ThemeColor::TertiaryText));
    m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_idLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_idLabel->setFixedHeight(QFontMetrics(secondaryFont).height() + 2);
    m_idPrefixLabel->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    m_idPrefixLabel->setFixedHeight(QFontMetrics(secondaryFont).height() + 2);
    m_nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_nameLabel->setFixedHeight(QFontMetrics(nameFont).height() + 2);

    auto* idRow = new QHBoxLayout;
    idRow->setContentsMargins(0, 0, 0, 0);
    idRow->setSpacing(5);
    idRow->addWidget(m_idPrefixLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addWidget(m_idLabel, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addWidget(m_copyIdButton, 0, Qt::AlignLeft | Qt::AlignVCenter);
    idRow->addStretch();
    m_idPrefixLabel->setVisible(false);
    m_copyIdButton->setEnabled(false);
    m_copyIdButton->setVisible(false);

    identity->addWidget(m_nameLabel);
    identity->addLayout(idRow);
    identity->addStretch();
    header->addLayout(identity, 1);
    contentLayout->addLayout(header);

    contentLayout->addItem(makeCompressibleSpacing(32));
    contentLayout->addSpacing(1);
    contentLayout->addItem(makeCompressibleSpacing(34));

    m_remarkEdit->setPlaceholderText(QStringLiteral("设置群备注"));
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
    connect(m_remarkEdit, &InlineEditableText::editingFinished, this, &GroupDetailPage::saveRemark);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("备注"), m_remarkEdit));
    contentLayout->addItem(makeCompressibleSpacing(24));

    auto* categoryRowHost = new QWidget(this);
    categoryRowHost->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    auto* categoryRow = new QHBoxLayout(categoryRowHost);
    categoryRow->setContentsMargins(0, 0, 0, 0);
    categoryRow->addStretch();
    categoryRow->addWidget(m_categoryButton, 0, Qt::AlignRight);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("群分组"), categoryRowHost));
    contentLayout->addItem(makeCompressibleSpacing(24));

    contentLayout->addLayout(makeInfoRow(QStringLiteral("群介绍"), m_introLabel));
    contentLayout->addItem(makeCompressibleSpacing(24));
    contentLayout->addLayout(makeInfoRow(QStringLiteral("群公告"), m_announcementLabel));
    contentLayout->addItem(makeCompressibleSpacing(24));
    contentLayout->addLayout(makeInfoRow(QStringLiteral("群人数"), m_memberCountLabel));

    contentLayout->addItem(makeCompressibleSpacing(58));
    auto* buttons = new QHBoxLayout;
    buttons->setContentsMargins(0, 0, 0, 0);
    buttons->setSpacing(10);
    buttons->addStretch();
    m_messageButton->setFixedSize(118, 38);
    m_exitButton->setFixedSize(118, 38);
    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_exitButton);
    buttons->addWidget(m_messageButton);
    buttons->addWidget(m_exitButton);
    buttons->addStretch();
    contentLayout->addLayout(buttons);

    connect(m_messageButton, &StatefulPushButton::clicked, this, [this]() {
        if (m_hasGroup) {
            emit requestMessage(m_group.groupId);
        }
    });
    connect(m_exitButton, &StatefulPushButton::clicked, this, [this]() {
        confirmExitGroup();
    });
    connect(m_copyIdButton, &QToolButton::clicked, this, &GroupDetailPage::copyCurrentId);

    connect(m_categoryButton, &QToolButton::clicked, this, &GroupDetailPage::showCategoryMenu);
    connect(m_categoryMenu, &StyledActionMenu::aboutToHide, this, [this]() {
        m_categoryButton->setDown(false);
        static_cast<ThemedSelectButton*>(m_categoryButton)->setMenuHoverSuppressed(true);
        m_categoryButton->update();
    });
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });

    qApp->installEventFilter(this);
    applyTheme();
}

GroupDetailPage::~GroupDetailPage()
{
    qApp->removeEventFilter(this);
}

QRect GroupDetailPage::avatarRect() const
{
    if (!m_contentWidget || !m_nameLabel) {
        return {};
    }

    const QPoint nameTopLeft = m_nameLabel->mapTo(const_cast<GroupDetailPage*>(this), QPoint(0, 0));
    const int avatarX = nameTopLeft.x() - kHeaderSpacing - kAvatarSize;
    const int avatarY = nameTopLeft.y() - kIdentityTopInset;
    return QRect(avatarX, avatarY, kAvatarSize, kAvatarSize);
}

void GroupDetailPage::setController(FriendSessionController* controller)
{
    if (m_controller) {
        disconnect(m_controller, nullptr, this, nullptr);
    }

    m_controller = controller;
    if (!m_controller) {
        return;
    }

    connect(m_controller, &FriendSessionController::groupAvatarImageReady,
            this, [this](const QString& requestId, const QString& groupId, const QImage& image) {
        if (requestId != m_avatarImageRequestId || !m_avatarViewer || groupId != m_group.groupId) {
            return;
        }
        m_avatarImageRequestId.clear();
        m_avatarViewer->replaceImage(image, m_avatarSource);
    });
    connect(m_controller, &FriendSessionController::groupAvatarImageFailed,
            this, [this](const QString& requestId, const QString&) {
        if (requestId == m_avatarImageRequestId) {
            m_avatarImageRequestId.clear();
        }
    });
}

void GroupDetailPage::setGroupId(const QString& groupId)
{
    if (groupId.isEmpty()) {
        clear();
        return;
    }
    setGroup(m_controller ? m_controller->loadGroup(groupId) : Group());
}

void GroupDetailPage::clear()
{
    m_group = {};
    m_hasGroup = false;
    m_avatarSource.clear();
    m_avatarImageRequestId.clear();
    m_nameLabel->clear();
    m_idPrefixLabel->setVisible(false);
    m_idLabel->clear();
    m_copyIdButton->setEnabled(false);
    m_copyIdButton->setVisible(false);
    m_introLabel->clear();
    m_announcementLabel->clear();
    m_memberCountLabel->clear();
    updateExitButtonState();
    update();
}

void GroupDetailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateIntroText();
    updateAnnouncementText();
}

void GroupDetailPage::paintEvent(QPaintEvent* event)
{
    QPainter painter(this);
    painter.fillRect(event->rect(), ThemeManager::instance().color(ThemeColor::PageBackground));

    if (!m_contentWidget) {
        return;
    }

    const QRect avatarRect = this->avatarRect();

    if (avatarRect.intersects(event->rect()) && !m_avatarSource.isEmpty()) {
        const QPixmap avatar = ImageService::instance().circularAvatar(m_avatarSource,
                                                                       kAvatarSize,
                                                                       devicePixelRatioF());
        if (avatar.isNull()) {
            MediaPlaceholderRenderer::drawAvatar(&painter, avatarRect);
        } else {
            painter.drawPixmap(avatarRect, avatar);
        }
    }

    const int separatorY = avatarRect.top() + kAvatarSize + kSeparatorTopSpacing;
    const QRect separatorRect(m_contentWidget->x(), separatorY, m_contentWidget->width(), 1);
    if (separatorRect.intersects(event->rect())) {
        painter.fillRect(separatorRect, ThemeManager::instance().color(ThemeColor::Divider));
    }
}

void GroupDetailPage::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton && avatarRect().contains(event->pos())) {
        openAvatarViewer();
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

bool GroupDetailPage::eventFilter(QObject* watched, QEvent* event)
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

void GroupDetailPage::setGroup(const Group& group)
{
    if (group.groupId.isEmpty()) {
        clear();
        return;
    }

    m_group = group;
    m_hasGroup = true;
    m_remarkEdit->finishEditing();

    m_nameLabel->setText(m_group.groupName);
    m_idPrefixLabel->setVisible(true);
    m_idLabel->setText(m_group.groupId);
    m_copyIdButton->setEnabled(true);
    m_copyIdButton->setVisible(true);
    updateAvatar();
    updateRemarkText();
    updateCategoryButtonText();
    updateIntroText();
    updateAnnouncementText();
    updateMemberCountText();
    updateExitButtonState();
    QTimer::singleShot(0, this, &GroupDetailPage::updateIntroText);
    QTimer::singleShot(0, this, &GroupDetailPage::updateAnnouncementText);
}

void GroupDetailPage::openAvatarViewer()
{
    if (!m_hasGroup || m_avatarSource.isEmpty()) {
        return;
    }

    QPixmap preview = ImageService::instance().circularAvatar(m_avatarSource, kAvatarSize, 1.0);
    if (preview.isNull()) {
        preview = ImageService::instance().pixmap(m_avatarSource);
    }
    if (preview.isNull()) {
        return;
    }

    auto* viewer = new ImageViewer(preview, m_avatarSource, window());
    m_avatarViewer = viewer;
    if (m_controller) {
        m_avatarImageRequestId = m_controller->requestGroupAvatarImage(m_group.groupId);
    }
    viewer->show();
    viewer->raise();
    viewer->activateWindow();
}

void GroupDetailPage::updateAvatar()
{
    m_avatarSource = m_group.groupAvatarPath;
    update();
}

void GroupDetailPage::applyTheme()
{
    const QList<PaintedLabel*> labels = findChildren<PaintedLabel*>();
    for (PaintedLabel* label : labels) {
        const QVariant roleValue = label->property("themeTextRole");
        if (!roleValue.isValid()) {
            continue;
        }
        const auto role = static_cast<ThemeColor>(roleValue.toInt());
        label->setTextColor(ThemeManager::instance().color(role));
    }

    m_remarkEdit->setTextColor(ThemeManager::instance().color(ThemeColor::SecondaryText));
    m_remarkEdit->setPlaceholderTextColor(ThemeManager::instance().color(ThemeColor::PlaceholderText));
    m_remarkEdit->setHoverBackgroundColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_remarkEdit->setFocusBackgroundColor(ThemeManager::instance().color(ThemeColor::PanelBackground));
    m_remarkEdit->setFocusBorderColor(ThemeManager::instance().color(ThemeColor::Divider));

    applyPrimaryButtonStyle(m_messageButton);
    applyDangerOutlineButtonStyle(m_exitButton);
    m_categoryButton->update();
    m_copyIdButton->update();
    update();
}

void GroupDetailPage::updateRemarkText()
{
    if (m_remarkEdit->text() != m_group.remark) {
        m_remarkEdit->setText(m_group.remark);
    }
}

void GroupDetailPage::updateCategoryButtonText()
{
    const QMap<QString, QString> categories = m_controller
            ? m_controller->loadGroupCategories()
            : QMap<QString, QString>();
    const QString categoryId = m_group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : m_group.listGroupId;
    const QString categoryName = categories.value(categoryId,
                                                  m_group.listGroupName.isEmpty()
                                                          ? QStringLiteral("我加入的群聊")
                                                          : m_group.listGroupName);
    m_categoryButton->setText(categoryName);
    m_categoryButton->setEnabled(true);
}

void GroupDetailPage::updateIntroText()
{
    m_introLabel->setText(elidedValueText(m_group.introduction, m_introLabel));
}

void GroupDetailPage::updateAnnouncementText()
{
    m_announcementLabel->setText(elidedValueText(m_group.announcement, m_announcementLabel));
}

void GroupDetailPage::updateMemberCountText()
{
    const QString text = QStringLiteral("%1人").arg(m_group.memberNum);
    m_memberCountLabel->setText(text);
}

void GroupDetailPage::copyCurrentId()
{
    if (!m_hasGroup || m_group.groupId.isEmpty()) {
        return;
    }

    QApplication::clipboard()->setText(m_group.groupId);
    GlobalNotification::showSuccess(this, QStringLiteral("复制成功"));
}

QString GroupDetailPage::elidedValueText(const QString& text, const QLabel* label) const
{
    if (!label) {
        return text;
    }

    const int availableWidth = qMax(0, label->width());
    return QFontMetrics(label->font()).elidedText(text, Qt::ElideRight, availableWidth);
}

void GroupDetailPage::saveRemark()
{
    if (!m_hasGroup) {
        return;
    }

    const QString remark = m_remarkEdit->text().trimmed();
    if (remark == m_group.remark) {
        return;
    }

    m_group.remark = remark;
    if (!m_controller || !m_controller->saveGroup(m_group)) {
        return;
    }
    updateRemarkText();
}

void GroupDetailPage::showCategoryMenu()
{
    if (!m_hasGroup || !m_categoryButton->isEnabled()) {
        return;
    }

    static_cast<ThemedSelectButton*>(m_categoryButton)->setMenuHoverSuppressed(false);
    rebuildCategoryMenu();
    m_categoryMenu->setFixedWidth(m_categoryButton->width());
    m_categoryMenu->popup(m_categoryButton->mapToGlobal(QPoint(0, m_categoryButton->height())));
    m_categoryButton->setDown(false);
    static_cast<ThemedSelectButton*>(m_categoryButton)->setMenuHoverSuppressed(true);
    m_categoryButton->update();
}

void GroupDetailPage::rebuildCategoryMenu()
{
    StyledActionMenu* menu = m_categoryMenu;
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

    const QMap<QString, QString> categories = m_controller
            ? m_controller->loadGroupCategories()
            : QMap<QString, QString>();
    auto addCategoryAction = [this, menu, group](const QString& categoryId, const QString& categoryName) {
        QAction* action = menu->addAction(categoryName);
        action->setCheckable(true);
        const QString currentCategoryId = m_group.listGroupId.isEmpty()
                ? QStringLiteral("gg_joined")
                : m_group.listGroupId;
        action->setChecked(categoryId == currentCategoryId);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, categoryId, categoryName]() {
            changeCategory(categoryId, categoryName);
        });
    };

    for (const QString& categoryId : editableCategoryOrder()) {
        addCategoryAction(categoryId, categories.value(categoryId));
    }

}

void GroupDetailPage::changeCategory(const QString& categoryId, const QString& categoryName)
{
    const QString currentCategoryId = m_group.listGroupId.isEmpty()
            ? QStringLiteral("gg_joined")
            : m_group.listGroupId;
    if (!m_hasGroup || categoryId == currentCategoryId) {
        return;
    }

    m_group.listGroupId = categoryId;
    m_group.listGroupName = categoryName;
    if (!m_controller || !m_controller->saveGroup(m_group)) {
        return;
    }
    updateCategoryButtonText();
}

void GroupDetailPage::updateExitButtonState()
{
    const bool canExit = m_hasGroup && m_controller && m_controller->canExitGroup(m_group);
    m_exitButton->setEnabled(canExit);
}

void GroupDetailPage::confirmExitGroup()
{
    if (!m_hasGroup || !m_controller || !m_controller->canExitGroup(m_group)) {
        return;
    }

    const InWindowPopup::Button result = InWindowPopup::question(this,
                                                                 QStringLiteral("退出群聊"),
                                                                 QStringLiteral("确认退出该群聊吗？"));
    if (result != InWindowPopup::Button::Yes) {
        return;
    }

    if (m_controller->exitGroup(m_group.groupId)) {
        clear();
        emit groupExited();
    }
}
