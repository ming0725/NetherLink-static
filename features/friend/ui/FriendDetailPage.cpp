#include "FriendDetailPage.h"

#include <QActionGroup>
#include <QApplication>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPainter>
#include <QResizeEvent>
#include <QScrollArea>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include "features/friend/data/UserRepository.h"
#include "shared/services/ImageService.h"
#include "shared/ui/StatefulPushButton.h"

namespace {

constexpr int kAvatarSize = 88;
constexpr int kContentMaxWidth = 660;

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
    button->setNormalColor(Qt::white);
    button->setHoverColor(QColor(0xff, 0xf4, 0xf4));
    button->setPressColor(QColor(0xff, 0xe8, 0xe8));
    button->setTextColor(QColor(0xd9, 0x36, 0x36));
    button->setBorderColor(QColor(0xdd, 0xdd, 0xdd));
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
        setPopupMode(QToolButton::InstantPopup);
        setToolButtonStyle(Qt::ToolButtonTextOnly);
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(QPen(QColor(0xd8, 0xd8, 0xd8), 1));
        painter.setBrush(underMouse() ? QColor(0xee, 0xee, 0xee) : QColor(0xf7, 0xf7, 0xf7));
        painter.drawRoundedRect(rect().adjusted(0, 0, -1, -1), 5, 5);

        QFont textFont = font();
        textFont.setPixelSize(14);
        painter.setFont(textFont);
        painter.setPen(QColor(0x22, 0x22, 0x22));
        const QRect textRect = rect().adjusted(14, 0, -34, 0);
        painter.drawText(textRect,
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetrics(textFont).elidedText(text(), Qt::ElideRight, textRect.width()));

        const int centerX = width() - 19;
        const int centerY = height() / 2 + 1;
        QPen arrowPen(QColor(0x9a, 0x9a, 0x9a), 1.8, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter.setPen(arrowPen);
        painter.drawLine(QPointF(centerX - 6.0, centerY - 3.0), QPointF(centerX, centerY + 4.0));
        painter.drawLine(QPointF(centerX, centerY + 4.0), QPointF(centerX + 6.0, centerY - 3.0));
    }
};

FriendDetailPage::FriendDetailPage(QWidget* parent)
    : QWidget(parent)
    , m_avatarLabel(new AvatarLabel(this))
    , m_nameLabel(new QLabel(this))
    , m_idLabel(new QLabel(this))
    , m_regionLabel(new QLabel(this))
    , m_statusIcon(new QLabel(this))
    , m_statusLabel(new QLabel(this))
    , m_remarkEdit(new QLineEdit(this))
    , m_groupButton(new GroupSelectButton(this))
    , m_signatureLabel(new QLabel(this))
    , m_messageButton(new StatefulPushButton(QStringLiteral("发消息"), this))
    , m_deleteButton(new StatefulPushButton(QStringLiteral("删除好友"), this))
{
    setAutoFillBackground(true);
    QPalette pal = palette();
    pal.setColor(QPalette::Window, QColor(0xf7, 0xf7, 0xf7));
    setPalette(pal);

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
    m_nameLabel->setStyleSheet(QStringLiteral("color: #111111;"));

    QFont secondaryFont = m_idLabel->font();
    secondaryFont.setPixelSize(13);
    m_idLabel->setFont(secondaryFont);
    m_regionLabel->setFont(secondaryFont);
    m_statusLabel->setFont(secondaryFont);
    m_idLabel->setStyleSheet(QStringLiteral("color: #777777;"));
    m_regionLabel->setStyleSheet(QStringLiteral("color: #777777;"));
    m_statusLabel->setStyleSheet(QStringLiteral("color: #222222;"));

    auto* statusRow = new QHBoxLayout;
    statusRow->setContentsMargins(0, 0, 0, 0);
    statusRow->setSpacing(6);
    m_statusIcon->setFixedSize(14, 14);
    statusRow->addWidget(m_statusIcon);
    statusRow->addWidget(m_statusLabel);
    statusRow->addStretch();

    identity->addWidget(m_nameLabel);
    identity->addWidget(m_idLabel);
    identity->addWidget(m_regionLabel);
    identity->addLayout(statusRow);
    identity->addStretch();
    header->addLayout(identity, 1);
    contentLayout->addLayout(header);

    contentLayout->addSpacing(54);
    contentLayout->addWidget(makeSeparator());
    contentLayout->addSpacing(34);

    m_remarkEdit->setPlaceholderText(QStringLiteral("设置好友备注"));
    m_remarkEdit->setFixedHeight(34);
    m_remarkEdit->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_remarkEdit->setStyleSheet(QStringLiteral(
            "QLineEdit { border: 1px solid transparent; border-radius: 5px; background: transparent; color: #555555; padding: 0 8px; }"
            "QLineEdit:hover { background: #eeeeee; }"
            "QLineEdit:focus { border-color: #d8d8d8; background: #ffffff; }"));
    connect(m_remarkEdit, &QLineEdit::editingFinished, this, &FriendDetailPage::saveRemark);
    contentLayout->addLayout(makeInfoRow(QStringLiteral("备注"), m_remarkEdit));
    contentLayout->addSpacing(28);

    auto* groupRowHost = new QWidget(this);
    auto* groupRow = new QHBoxLayout(groupRowHost);
    groupRow->setContentsMargins(0, 0, 0, 0);
    groupRow->addWidget(m_groupButton, 0, Qt::AlignLeft);
    groupRow->addStretch();
    contentLayout->addLayout(makeInfoRow(QStringLiteral("好友分组"), groupRowHost));
    contentLayout->addSpacing(28);

    m_signatureLabel->setStyleSheet(QStringLiteral("color: #222222;"));
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
        if (!m_hasUser) {
            return;
        }
        UserRepository::instance().removeUser(m_user.id);
        clear();
        emit friendDeleted();
    });

    qApp->installEventFilter(this);
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
}

void FriendDetailPage::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateSignatureText();
}

bool FriendDetailPage::eventFilter(QObject* watched, QEvent* event)
{
    if (event->type() == QEvent::MouseButtonPress && m_remarkEdit->hasFocus()) {
        auto* widget = qobject_cast<QWidget*>(watched);
        if (widget && widget != m_remarkEdit && !m_remarkEdit->isAncestorOf(widget)) {
            saveRemark();
            m_remarkEdit->clearFocus();
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

    m_nameLabel->setText(m_user.nick);
    m_idLabel->setText(QStringLiteral("ID %1").arg(m_user.id));
    m_regionLabel->setText(m_user.region);
    m_regionLabel->setVisible(!m_user.region.isEmpty());
    m_statusLabel->setText(statusText(m_user.status));
    updateAvatar();
    updateRemarkText();
    updateGroupButtonText();
    updateSignatureText();
    QTimer::singleShot(0, this, &FriendDetailPage::updateSignatureText);
    rebuildGroupMenu();
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

void FriendDetailPage::rebuildGroupMenu()
{
    auto* menu = new QMenu(m_groupButton);
    menu->setStyleSheet(QStringLiteral(
            "QMenu { background: #f0f0f0; border: 1px solid #d6d6d6; padding: 4px; }"
            "QMenu::item { padding: 6px 26px 6px 24px; border-radius: 4px; }"
            "QMenu::item:selected { background: #e2e2e2; }"));

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

    if (QMenu* oldMenu = m_groupButton->menu()) {
        oldMenu->deleteLater();
    }
    m_groupButton->setMenu(menu);
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
    rebuildGroupMenu();
}
