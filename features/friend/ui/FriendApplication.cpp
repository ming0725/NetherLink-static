#include "FriendApplication.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/ui/TransparentSplitter.h"
#include "shared/theme/ThemeManager.h"
#include <QPainter>
#include <QPaintEvent>
#include <QPushButton>
#include <QResizeEvent>
#include <QVariantAnimation>

namespace {

constexpr int kLeftPaneWidth = 200;

class ModeSwitchBar : public QWidget
{
public:
    explicit ModeSwitchBar(QWidget* parent = nullptr)
        : QWidget(parent)
        , m_animation(new QVariantAnimation(this))
    {
        setAttribute(Qt::WA_StyledBackground, false);
        m_animation->setDuration(180);
        m_animation->setEasingCurve(QEasingCurve::OutCubic);
        connect(m_animation, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
            m_thumbProgress = value.toReal();
            update();
        });
    }

    void setButtons(QPushButton* friendButton, QPushButton* groupButton)
    {
        m_friendButton = friendButton;
        m_groupButton = groupButton;
    }

    void setCurrentSegment(int segment, bool animate)
    {
        const qreal targetProgress = segment == 0 ? 0.0 : 1.0;
        if (qAbs(m_thumbProgress - targetProgress) <= 0.001) {
            return;
        }

        m_animation->stop();
        if (!animate) {
            m_thumbProgress = targetProgress;
            update();
            return;
        }

        m_animation->setStartValue(m_thumbProgress);
        m_animation->setEndValue(targetProgress);
        m_animation->start();
    }

protected:
    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::PageBackground));
        painter.drawRoundedRect(rect(), 6, 6);

        const QRectF friendRect = segmentRect(m_friendButton);
        const QRectF groupRect = segmentRect(m_groupButton);
        if (friendRect.isEmpty() || groupRect.isEmpty()) {
            return;
        }

        const qreal x = friendRect.x() + (groupRect.x() - friendRect.x()) * m_thumbProgress;
        const qreal width = friendRect.width() + (groupRect.width() - friendRect.width()) * m_thumbProgress;
        const QRectF thumbRect(x, friendRect.y(), width, friendRect.height());
        painter.setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
        painter.drawRoundedRect(thumbRect, 4, 4);
    }

private:
    QRectF segmentRect(const QPushButton* button) const
    {
        return button ? QRectF(button->geometry()) : QRectF();
    }

    QPushButton* m_friendButton = nullptr;
    QPushButton* m_groupButton = nullptr;
    QVariantAnimation* m_animation = nullptr;
    qreal m_thumbProgress = 0.0;
};

class ModeSegmentButton final : public QPushButton
{
public:
    explicit ModeSegmentButton(const QString& text, QWidget* parent = nullptr)
        : QPushButton(text, parent)
    {
        setCheckable(true);
        setAutoExclusive(true);
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
        setAttribute(Qt::WA_Hover);
        setFlat(true);
    }

protected:
    bool event(QEvent* event) override
    {
        switch (event->type()) {
        case QEvent::HoverEnter:
        case QEvent::HoverLeave:
        case QEvent::MouseButtonPress:
        case QEvent::MouseButtonRelease:
        case QEvent::EnabledChange:
            update();
            break;
        default:
            break;
        }
        return QPushButton::event(event);
    }

    void paintEvent(QPaintEvent* event) override
    {
        Q_UNUSED(event);

        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);

        QFont textFont = font();
        textFont.setPixelSize(12);
        textFont.setWeight(isChecked() ? QFont::Medium : QFont::Normal);
        painter.setFont(textFont);

        QColor textColor = ThemeManager::instance().color(ThemeColor::SecondaryText);
        if (isChecked()) {
            textColor = ThemeManager::instance().color(ThemeColor::Accent);
        } else if (underMouse()) {
            textColor = ThemeManager::instance().color(ThemeColor::PrimaryText);
        }
        painter.setPen(textColor);
        painter.drawText(rect(), Qt::AlignCenter, text());
    }
};

} // namespace

FriendApplication::LeftPane::LeftPane(QWidget* parent)
        : QWidget(parent)
        , m_searchInput(new IconLineEdit(this))
        , m_addButton(new StatefulPushButton("+", this))
        , m_modeBar(new ModeSwitchBar(this))
        , m_friendModeButton(new ModeSegmentButton(QStringLiteral("好友"), m_modeBar))
        , m_groupModeButton(new ModeSegmentButton(QStringLiteral("群聊"), m_modeBar))
        , m_content(new FriendListWidget(this))
        , m_groupList(new GroupListWidget(this))
{
    setFixedWidth(kLeftPaneWidth);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_searchInput->setFixedHeight(26);
    m_addButton->setRadius(8);
    m_addButton->setFixedHeight(26);

    QFont addFont = m_addButton->font();
    addFont.setPixelSize(18);
    m_addButton->setFont(addFont);

    m_friendModeButton->setChecked(true);
    static_cast<ModeSwitchBar*>(m_modeBar)->setButtons(m_friendModeButton, m_groupModeButton);

    m_groupList->hide();

    applyTheme();
    connect(&ThemeManager::instance(), &ThemeManager::themeChanged, this, [this]() {
        applyTheme();
    });

    connect(m_addButton, &StatefulPushButton::clicked, this, [this]() {
        auto* menu = new StyledActionMenu(this);
        menu->setItemHoverColor(QColor(238, 238, 238));
        menu->addAction(QStringLiteral("添加好友"));
        menu->addAction(QStringLiteral("添加群聊"));
        menu->addAction(QStringLiteral("创建群聊"));

        connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
            m_addButton->setPressedVisual(false);
            menu->deleteLater();
        });

        m_addButton->setPressedVisual(true);
        const int popupGap = menu->isUsingNativeMenu() ? 10 : 0;
        const QPoint popupPos = m_addButton->mapToGlobal(
                QPoint(0, m_addButton->height() + popupGap));
        menu->popup(popupPos);
    });

    connect(m_friendModeButton, &QPushButton::clicked, this, [this]() { updateContentMode(true); });
    connect(m_groupModeButton, &QPushButton::clicked, this, [this]() { updateContentMode(true); });
}

void FriendApplication::LeftPane::applyTheme()
{
    m_addButton->setNormalColor(ThemeManager::instance().color(ThemeColor::InputBackground));
    m_addButton->setHoverColor(ThemeManager::instance().color(ThemeColor::ListHover));
    m_addButton->setPressColor(ThemeManager::instance().color(ThemeColor::Divider));
    m_addButton->setTextColor(ThemeManager::instance().color(ThemeColor::PrimaryText));

    m_friendModeButton->update();
    m_groupModeButton->update();
    m_modeBar->update();
    update();
}

void FriendApplication::LeftPane::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);

    const int topMargin = 24;
    const int leftMargin = 14;
    const int rightMargin = 14;
    const int spacing = 5;
    const int controlHeight = 26;
    const int buttonX = width() - rightMargin - controlHeight;
    const int modeHeight = 32;

    m_addButton->setGeometry(buttonX, topMargin, controlHeight, controlHeight);
    m_searchInput->setGeometry(leftMargin,
                               topMargin,
                               qMax(0, buttonX - spacing - leftMargin),
                               controlHeight);

    int nextY = topMargin + controlHeight + 12;
    m_modeBar->setGeometry(leftMargin, nextY, qMax(0, width() - leftMargin - rightMargin), modeHeight);
    const int modeInset = 4;
    const int modeButtonWidth = qMax(0, (m_modeBar->width() - modeInset * 2) / 2);
    m_friendModeButton->setGeometry(modeInset, modeInset, modeButtonWidth, modeHeight - modeInset * 2);
    m_groupModeButton->setGeometry(modeInset + modeButtonWidth, modeInset,
                                   qMax(0, m_modeBar->width() - modeInset * 2 - modeButtonWidth),
                                   modeHeight - modeInset * 2);
    m_modeBar->update();

    const int contentY = nextY + modeHeight + 4;
    const QRect contentRect(0, contentY, width(), qMax(0, height() - contentY));
    m_content->setGeometry(contentRect);
    m_groupList->setGeometry(contentRect);
    updateContentMode(false);
}

void FriendApplication::LeftPane::updateContentMode(bool animate)
{
    const bool showGroups = m_groupModeButton->isChecked();
    static_cast<ModeSwitchBar*>(m_modeBar)->setCurrentSegment(showGroups ? 1 : 0, animate);
    m_content->setVisible(!showGroups);
    m_groupList->setVisible(showGroups);
    if (showGroups) {
        m_groupList->ensureInitialized();
    }
}

FriendApplication::FriendApplication(QWidget* parent)
        : QWidget(parent)
{
    // 1) 左侧面板：内部手动布局搜索框、加号按钮和好友列表
    m_leftPane    = new LeftPane(this);

    // 2) 右侧默认页 + 单实例好友详情页
    m_rightStack = new QStackedWidget(this);
    m_defaultPage = new DefaultPage(this);
    m_detailPage = new FriendDetailPage(this);
    m_groupDetailPage = new GroupDetailPage(this);
    m_rightStack->setMinimumWidth(0);
    m_rightStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_rightStack->addWidget(m_defaultPage);
    m_rightStack->addWidget(m_detailPage);
    m_rightStack->addWidget(m_groupDetailPage);
    m_rightStack->setCurrentWidget(m_defaultPage);

    // 3) 分割器：将左、右面板加入
    m_splitter    = new TransparentSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_rightStack);
    m_splitter->setHandleWidth(0);               // >0，允许拖拽
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 0);          // 左侧不自动扩展
    m_splitter->setStretchFactor(1, 1);          // 右侧占满剩余

    // （可选）设置初始左侧宽度
    m_splitter->setSizes({ kLeftPaneWidth, qMax(0, width() - kLeftPaneWidth) });
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    // 去除窗口自带标题栏，若需要无边框效果
    setWindowFlag(Qt::FramelessWindowHint);

    connect(m_leftPane->searchInput()->getLineEdit(), &QLineEdit::textChanged,
            m_leftPane->friendList(), &FriendListWidget::setKeyword);
    connect(m_leftPane->searchInput()->getLineEdit(), &QLineEdit::textChanged,
            m_leftPane->groupList(), &GroupListWidget::setKeyword);
    connect(m_leftPane->friendList(), &FriendListWidget::selectedFriendChanged,
            this, [this](const QString& userId) {
                if (userId.isEmpty()) {
                    m_rightStack->setCurrentWidget(m_defaultPage);
                    return;
                }
                m_detailPage->setUserId(userId);
                m_rightStack->setCurrentWidget(m_detailPage);
            });
    connect(m_leftPane->groupList(), &GroupListWidget::selectedGroupChanged,
            this, [this](const QString& groupId) {
                if (groupId.isEmpty()) {
                    m_rightStack->setCurrentWidget(m_defaultPage);
                    return;
                }
                m_groupDetailPage->setGroupId(groupId);
                m_rightStack->setCurrentWidget(m_groupDetailPage);
            });
    connect(m_leftPane->friendList(), &FriendListWidget::requestMessage,
            this, &FriendApplication::requestOpenConversation);
    connect(m_leftPane->groupList(), &GroupListWidget::requestMessage,
            this, &FriendApplication::requestOpenConversation);
    connect(m_detailPage, &FriendDetailPage::requestMessage,
            this, &FriendApplication::requestOpenConversation);
    connect(m_groupDetailPage, &GroupDetailPage::requestMessage,
            this, &FriendApplication::requestOpenConversation);
    connect(m_detailPage, &FriendDetailPage::friendDeleted,
            this, [this]() {
                m_rightStack->setCurrentWidget(m_defaultPage);
            });
    connect(m_groupDetailPage, &GroupDetailPage::groupExited,
            this, [this]() {
                m_rightStack->setCurrentWidget(m_defaultPage);
            });
}

void FriendApplication::resizeEvent(QResizeEvent* /*event*/)
{
    // 把 splitter 铺满整个 FriendApplication
    m_splitter->setGeometry(0, 0, width(), height());
}

void FriendApplication::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(ThemeManager::instance().color(ThemeColor::PanelBackground));
    p.drawRect(rect());
}
