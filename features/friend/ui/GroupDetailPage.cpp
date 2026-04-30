#include "GroupDetailPage.h"

#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMap>
#include <QMessageBox>
#include <QPainter>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "features/chat/data/GroupRepository.h"
#include "features/chat/data/MessageRepository.h"
#include "shared/services/ImageService.h"
#include "shared/ui/InlineEditableText.h"
#include "shared/ui/StatefulPushButton.h"

namespace {

constexpr int kAvatarSize = 88;
constexpr int kInfoTitleWidth = 96;
constexpr int kInfoColumnSpacing = 24;

QLabel* makeTitleLabel(const QString& text)
{
    auto* label = new QLabel(text);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: #222222;"));
    return label;
}

QFrame* makeSeparator()
{
    auto* line = new QFrame;
    line->setFrameShape(QFrame::HLine);
    line->setFrameShadow(QFrame::Plain);
    line->setFixedHeight(1);
    line->setStyleSheet(QStringLiteral("color: #dedede; background: #dedede; border: none;"));
    return line;
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

QLabel* makeValueLabel(QWidget* parent)
{
    auto* label = new QLabel(parent);
    QFont font = label->font();
    font.setPixelSize(14);
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: #222222;"));
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
    button->setNormalColor(Qt::white);
    button->setHoverColor(QColor(0xff, 0xf4, 0xf4));
    button->setPressColor(QColor(0xff, 0xe8, 0xe8));
    button->setTextColor(QColor(0xd9, 0x36, 0x36));
    button->setBorderColor(QColor(0xdd, 0xdd, 0xdd));
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

class GroupDetailPage::AvatarLabel : public QLabel
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

class CategorySelectButton : public QToolButton
{
public:
    explicit CategorySelectButton(QWidget* parent = nullptr)
        : QToolButton(parent)
    {
        setFixedSize(172, 34);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setPopupMode(QToolButton::InstantPopup);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        const QColor borderColor = isEnabled() ? QColor(0xd8, 0xd8, 0xd8) : QColor(0xe2, 0xe2, 0xe2);
        const QColor bgColor = isEnabled()
                ? (underMouse() ? QColor(0xee, 0xee, 0xee) : QColor(0xf7, 0xf7, 0xf7))
                : QColor(0xf4, 0xf4, 0xf4);
        painter.setPen(QPen(borderColor, 1));
        painter.setBrush(bgColor);
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

        QFont textFont = font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);
        painter.setPen(isEnabled() ? QColor(0x22, 0x22, 0x22) : QColor(0x88, 0x88, 0x88));
        const QRect textRect = rect().adjusted(14, 0, -34, 0);
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(textFont).elidedText(text(), Qt::ElideRight, textRect.width()));

        if (!isEnabled()) {
            return;
        }

        const int centerX = width() - 19;
        const int centerY = height() / 2 + 1;
        QPen arrowPen(QColor(0x9a, 0x9a, 0x9a), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(centerX - 6.0, centerY - 3.0), QPointF(centerX, centerY + 4.0));
        painter.drawLine(QPointF(centerX, centerY + 4.0), QPointF(centerX + 6.0, centerY - 3.0));
    }
};

GroupDetailPage::GroupDetailPage(QWidget* parent)
    : QWidget(parent)
    , m_avatarLabel(new AvatarLabel(this))
    , m_nameLabel(new QLabel(this))
    , m_idLabel(new QLabel(this))
    , m_remarkEdit(new InlineEditableText(this))
    , m_categoryButton(new CategorySelectButton(this))
    , m_introLabel(makeValueLabel(this))
    , m_announcementLabel(makeValueLabel(this))
    , m_memberCountLabel(makeValueLabel(this))
    , m_messageButton(new StatefulPushButton(QStringLiteral("发消息"), this))
    , m_exitButton(new StatefulPushButton(QStringLiteral("退出群聊"), this))
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xf7, 0xf7, 0xf7));
    setPalette(pal);

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(88, 64, 88, 40);
    root->setSpacing(0);

    auto* content = new QWidget(this);
    content->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(0, 0, 0, 0);
    contentLayout->setSpacing(0);
    root->addWidget(content);
    root->addStretch(1);

    auto* header = new QHBoxLayout;
    header->setContentsMargins(0, 0, 0, 0);
    header->setSpacing(30);
    header->addWidget(m_avatarLabel, 0, Qt::AlignTop);

    auto* identity = new QVBoxLayout;
    identity->setContentsMargins(0, 6, 0, 0);
    identity->setSpacing(8);

    QFont nameFont = m_nameLabel->font();
    nameFont.setPixelSize(20);
    nameFont.setWeight(QFont::DemiBold);
    m_nameLabel->setFont(nameFont);
    m_nameLabel->setStyleSheet(QStringLiteral("color: #111111;"));
    m_nameLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    QFont secondaryFont = m_idLabel->font();
    secondaryFont.setPixelSize(13);
    m_idLabel->setFont(secondaryFont);
    m_idLabel->setStyleSheet(QStringLiteral("color: #777777;"));
    m_idLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_idLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_idLabel->setFixedHeight(QFontMetrics(secondaryFont).height() + 2);
    m_nameLabel->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
    m_nameLabel->setFixedHeight(QFontMetrics(nameFont).height() + 2);

    identity->addWidget(m_nameLabel);
    identity->addWidget(m_idLabel);
    identity->addStretch();
    header->addLayout(identity, 1);
    contentLayout->addLayout(header);

    contentLayout->addItem(makeCompressibleSpacing(32));
    contentLayout->addWidget(makeSeparator());
    contentLayout->addItem(makeCompressibleSpacing(34));

    m_remarkEdit->setPlaceholderText(QStringLiteral("设置群备注"));
    m_remarkEdit->setFocusPolicy(Qt::ClickFocus);
    m_remarkEdit->setFixedHeight(34);
    m_remarkEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_remarkEdit->setTextColor(QColor(0x55, 0x55, 0x55));
    m_remarkEdit->setPlaceholderTextColor(QColor(0x88, 0x88, 0x88));
    m_remarkEdit->setNormalBackgroundColor(Qt::transparent);
    m_remarkEdit->setHoverBackgroundColor(QColor(0xEE, 0xEE, 0xEE));
    m_remarkEdit->setFocusBackgroundColor(Qt::white);
    m_remarkEdit->setNormalBorderColor(Qt::transparent);
    m_remarkEdit->setFocusBorderColor(QColor(0xD8, 0xD8, 0xD8));
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

    auto* categoryMenu = new QMenu(m_categoryButton);
    categoryMenu->setStyleSheet(QStringLiteral(
            "QMenu { background: #f0f0f0; border: 1px solid #d6d6d6; padding: 4px; }"
            "QMenu::item { padding: 6px 26px 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background: #e2e2e2; }"));
    connect(categoryMenu, &QMenu::aboutToShow, this, &GroupDetailPage::rebuildCategoryMenu);
    m_categoryButton->setMenu(categoryMenu);

    qApp->installEventFilter(this);
}

GroupDetailPage::~GroupDetailPage()
{
    qApp->removeEventFilter(this);
}

void GroupDetailPage::setGroupId(const QString& groupId)
{
    if (groupId.isEmpty()) {
        clear();
        return;
    }
    setGroup(GroupRepository::instance().requestGroupDetail({groupId}));
}

void GroupDetailPage::clear()
{
    m_group = {};
    m_hasGroup = false;
    updateExitButtonState();
}

void GroupDetailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateIntroText();
    updateAnnouncementText();
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
    m_idLabel->setText(QStringLiteral("ID %1").arg(m_group.groupId));
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

void GroupDetailPage::updateAvatar()
{
    m_avatarLabel->setAvatarSource(m_group.groupAvatarPath);
}

void GroupDetailPage::updateRemarkText()
{
    if (m_remarkEdit->text() != m_group.remark) {
        m_remarkEdit->setText(m_group.remark);
    }
}

void GroupDetailPage::updateCategoryButtonText()
{
    const QMap<QString, QString> categories = GroupRepository::instance().requestGroupCategories();
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
    GroupRepository::instance().saveGroup(m_group);
    updateRemarkText();
}

void GroupDetailPage::rebuildCategoryMenu()
{
    QMenu* menu = m_categoryButton->menu();
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

    const QMap<QString, QString> categories = GroupRepository::instance().requestGroupCategories();
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
    GroupRepository::instance().saveGroup(m_group);
    updateCategoryButtonText();
}

void GroupDetailPage::updateExitButtonState()
{
    const bool canExit = m_hasGroup && !GroupRepository::instance().isCurrentUserGroupOwner(m_group);
    m_exitButton->setEnabled(canExit);
}

void GroupDetailPage::confirmExitGroup()
{
    if (!m_hasGroup || GroupRepository::instance().isCurrentUserGroupOwner(m_group)) {
        return;
    }

    const int result = QMessageBox::question(this,
                                             QStringLiteral("退出群聊"),
                                             QStringLiteral("确认退出该群聊吗？"),
                                             QMessageBox::Yes | QMessageBox::No,
                                             QMessageBox::No);
    if (result != QMessageBox::Yes) {
        return;
    }

    MessageRepository::instance().removeConversation(m_group.groupId);
    GroupRepository::instance().removeGroup(m_group.groupId);
    clear();
    emit groupExited();
}
