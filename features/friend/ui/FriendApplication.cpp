#include "FriendApplication.h"
#include "shared/ui/TransparentSplitter.h"
#include "shared/ui/OverlayScrollListView.h"
#include <QAbstractListModel>
#include <QApplication>
#include <QPainter>
#include <QPaintEvent>
#include <QPalette>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollBar>
#include <QStyledItemDelegate>
#include <QStringList>
#include <QVariantAnimation>

extern const int kContactGroupArrowYOffset;

namespace {

constexpr int kSideListHeaderHeight = 36;
constexpr int kSideListLeftPadding = 12;
constexpr int kSideListCountRightPadding = 18;
constexpr int kSideListArrowSize = 12;
const int kNoticeArrowYOffset = 2;

const QFont& sideListFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(12);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& sideListMetrics()
{
    static const QFontMetrics metrics(sideListFont());
    return metrics;
}

const QFont& sideListCountFont()
{
    static const QFont font = []() {
        QFont value = QApplication::font();
        value.setPixelSize(11);
        value.setWeight(QFont::Medium);
        return value;
    }();
    return font;
}

const QFontMetrics& sideListCountMetrics()
{
    static const QFontMetrics metrics(sideListCountFont());
    return metrics;
}

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
        painter.setBrush(QColor(0xf2, 0xf2, 0xf2));
        painter.drawRoundedRect(rect(), 6, 6);

        const QRectF friendRect = segmentRect(m_friendButton);
        const QRectF groupRect = segmentRect(m_groupButton);
        if (friendRect.isEmpty() || groupRect.isEmpty()) {
            return;
        }

        const qreal x = friendRect.x() + (groupRect.x() - friendRect.x()) * m_thumbProgress;
        const qreal width = friendRect.width() + (groupRect.width() - friendRect.width()) * m_thumbProgress;
        const QRectF thumbRect(x, friendRect.y(), width, friendRect.height());
        painter.setBrush(Qt::white);
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

class GroupListPreviewModel : public QAbstractListModel
{
public:
    enum Role {
        IsNoticeRole = Qt::UserRole + 1,
        IsGroupRole,
        TitleRole,
        CountRole
    };

    explicit GroupListPreviewModel(QObject* parent = nullptr)
        : QAbstractListModel(parent)
        , m_groups({
              QStringLiteral("我创建的群聊"),
              QStringLiteral("我管理的群聊"),
              QStringLiteral("我加入的群聊"),
              QStringLiteral("最近联系群聊")
          })
    {
    }

    int rowCount(const QModelIndex& parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : m_groups.size() + 1;
    }

    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override
    {
        if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
            return {};
        }

        const bool isNotice = index.row() == 0;
        switch (role) {
        case Qt::DisplayRole:
        case TitleRole:
            return isNotice ? QStringLiteral("群通知") : m_groups.at(index.row() - 1);
        case IsNoticeRole:
            return isNotice;
        case IsGroupRole:
            return !isNotice;
        case CountRole:
            return 0;
        case Qt::SizeHintRole:
            return QSize(0, kSideListHeaderHeight);
        default:
            return {};
        }
    }

    Qt::ItemFlags flags(const QModelIndex& index) const override
    {
        return index.isValid() ? Qt::ItemIsEnabled : Qt::NoItemFlags;
    }

private:
    QStringList m_groups;
};

class GroupListPreviewDelegate : public QStyledItemDelegate
{
public:
    explicit GroupListPreviewDelegate(QObject* parent = nullptr)
        : QStyledItemDelegate(parent)
    {
    }

    void paint(QPainter* painter,
               const QStyleOptionViewItem& option,
               const QModelIndex& index) const override
    {
        painter->save();
        painter->setClipRect(option.rect);

        const bool hovered = option.state & QStyle::State_MouseOver;
        if (hovered) {
            const QRect hoverRect = option.rect.adjusted(6, 3, -6, -3);
            painter->setRenderHint(QPainter::Antialiasing, true);
            painter->setBrush(QColor(0xe9, 0xe9, 0xe9));
            painter->setPen(Qt::NoPen);
            painter->drawRoundedRect(hoverRect, 6, 6);
        }

        if (index.data(GroupListPreviewModel::IsNoticeRole).toBool()) {
            drawNotice(painter, option.rect, index.data(GroupListPreviewModel::TitleRole).toString());
            painter->restore();
            return;
        }

        drawGroup(painter, option.rect, index.data(GroupListPreviewModel::TitleRole).toString());
        painter->restore();
    }

    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override
    {
        Q_UNUSED(option);
        Q_UNUSED(index);
        return QSize(0, kSideListHeaderHeight);
    }

private:
    void drawNotice(QPainter* painter, const QRect& rect, const QString& title) const
    {
        painter->setFont(sideListFont());
        painter->setPen(QColor(0x11, 0x11, 0x11));
        painter->drawText(rect.adjusted(20, 0, -28, 0),
                          Qt::AlignLeft | Qt::AlignVCenter,
                          sideListMetrics().elidedText(title, Qt::ElideRight, qMax(0, rect.width() - 48)));

        const int centerX = rect.right() - kSideListCountRightPadding;
        const int centerY = rect.center().y() + kNoticeArrowYOffset;
        QPen arrowPen(QColor(0x66, 0x66, 0x66), 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(centerX - 2.0, centerY - 4.0), QPointF(centerX + 2.0, centerY));
        painter->drawLine(QPointF(centerX + 2.0, centerY), QPointF(centerX - 2.0, centerY + 4.0));
    }

    void drawGroup(QPainter* painter, const QRect& rect, const QString& title) const
    {
        const QRect arrowRect(rect.left() + kSideListLeftPadding,
                              rect.top() + (rect.height() - kSideListArrowSize) / 2
                                      + kContactGroupArrowYOffset,
                              kSideListArrowSize,
                              kSideListArrowSize);

        painter->save();
        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->translate(arrowRect.center());
        QPen arrowPen(QColor(0x78, 0x86, 0x94), 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        painter->setPen(arrowPen);
        painter->drawLine(QPointF(-2.5, -4.0), QPointF(2.5, 0.0));
        painter->drawLine(QPointF(2.5, 0.0), QPointF(-2.5, 4.0));
        painter->restore();

        const QString countText = QString::number(0);
        const int titleLeft = arrowRect.right() + 8;
        const int countWidth = sideListCountMetrics().horizontalAdvance(countText);
        const int rightEdge = rect.right() - kSideListCountRightPadding;
        const QRect countRect(qMax(titleLeft, rightEdge - countWidth + 1),
                              rect.top(),
                              countWidth,
                              rect.height());
        const QRect titleRect(titleLeft,
                              rect.top(),
                              qMax(0, countRect.left() - titleLeft - 8),
                              rect.height());

        painter->setFont(sideListFont());
        painter->setPen(QColor(0x38, 0x45, 0x52));
        painter->drawText(titleRect,
                          Qt::AlignLeft | Qt::AlignVCenter,
                          sideListMetrics().elidedText(title, Qt::ElideRight, titleRect.width()));
        painter->setFont(sideListCountFont());
        painter->setPen(QColor(0x99, 0xa3, 0xad));
        painter->drawText(countRect, Qt::AlignRight | Qt::AlignVCenter, countText);
    }
};

class GroupListPreviewWidget : public OverlayScrollListView
{
public:
    explicit GroupListPreviewWidget(QWidget* parent = nullptr)
        : OverlayScrollListView(parent)
        , m_model(new GroupListPreviewModel(this))
        , m_delegate(new GroupListPreviewDelegate(this))
    {
        setModel(m_model);
        setItemDelegate(m_delegate);
        setSelectionMode(QAbstractItemView::NoSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setMouseTracking(true);
        setAutoFillBackground(true);
        viewport()->setAutoFillBackground(true);
        QPalette palette = this->palette();
        palette.setColor(QPalette::Base, Qt::white);
        palette.setColor(QPalette::Window, Qt::white);
        setPalette(palette);
        viewport()->setPalette(palette);
        setWheelStepPixels(64);
        setScrollBarInsets(8, 4);
    }

private:
    GroupListPreviewModel* m_model;
    GroupListPreviewDelegate* m_delegate;
};

QString modeButtonStyle()
{
    return QStringLiteral(
            "QPushButton {"
            "border: none;"
            "border-radius: 4px;"
            "background: transparent;"
            "color: #606060;"
            "font-size: 12px;"
            "}"
            "QPushButton:checked {"
            "background: transparent;"
            "color: #0099ff;"
            "font-weight: 500;"
            "}"
            "QPushButton:hover:!checked {"
            "background: transparent;"
            "}");
}

} // namespace

FriendApplication::LeftPane::LeftPane(QWidget* parent)
        : QWidget(parent)
        , m_searchInput(new IconLineEdit(this))
        , m_addButton(new StatefulPushButton("+", this))
        , m_modeBar(new ModeSwitchBar(this))
        , m_friendModeButton(new QPushButton(QStringLiteral("好友"), m_modeBar))
        , m_groupModeButton(new QPushButton(QStringLiteral("群聊"), m_modeBar))
        , m_content(new FriendListWidget(this))
        , m_groupPlaceholder(new GroupListPreviewWidget(this))
{
    setMinimumWidth(144);
    setMaximumWidth(305);
    m_searchInput->setFixedHeight(26);
    m_addButton->setRadius(8);
    m_addButton->setNormalColor(QColor(0xF5, 0xF5, 0xF5));
    m_addButton->setHoverColor(QColor(0xEB, 0xEB, 0xEB));
    m_addButton->setPressColor(QColor(0xD7, 0xD7, 0xD7));
    m_addButton->setTextColor(QColor(0x33, 0x33, 0x33));
    m_addButton->setFixedHeight(26);

    QFont addFont = m_addButton->font();
    addFont.setPixelSize(18);
    m_addButton->setFont(addFont);

    m_friendModeButton->setCheckable(true);
    m_friendModeButton->setChecked(true);
    m_friendModeButton->setAutoExclusive(true);
    m_friendModeButton->setCursor(Qt::PointingHandCursor);
    m_friendModeButton->setFocusPolicy(Qt::NoFocus);
    m_friendModeButton->setStyleSheet(modeButtonStyle());
    m_groupModeButton->setCheckable(true);
    m_groupModeButton->setAutoExclusive(true);
    m_groupModeButton->setCursor(Qt::PointingHandCursor);
    m_groupModeButton->setFocusPolicy(Qt::NoFocus);
    m_groupModeButton->setStyleSheet(modeButtonStyle());
    static_cast<ModeSwitchBar*>(m_modeBar)->setButtons(m_friendModeButton, m_groupModeButton);

    m_groupPlaceholder->hide();

    connect(m_friendModeButton, &QPushButton::clicked, this, [this]() { updateContentMode(true); });
    connect(m_groupModeButton, &QPushButton::clicked, this, [this]() { updateContentMode(true); });
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
    m_groupPlaceholder->setGeometry(contentRect);
    updateContentMode(false);
}

void FriendApplication::LeftPane::updateContentMode(bool animate)
{
    const bool showGroups = m_groupModeButton->isChecked();
    static_cast<ModeSwitchBar*>(m_modeBar)->setCurrentSegment(showGroups ? 1 : 0, animate);
    m_content->setVisible(!showGroups);
    m_groupPlaceholder->setVisible(showGroups);
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
    m_rightStack->addWidget(m_defaultPage);
    m_rightStack->addWidget(m_detailPage);
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
    m_splitter->setSizes({ 200, width() - 200 });
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    // 去除窗口自带标题栏，若需要无边框效果
    setWindowFlag(Qt::FramelessWindowHint);

    connect(m_leftPane->searchInput()->getLineEdit(), &QLineEdit::textChanged,
            m_leftPane->friendList(), &FriendListWidget::setKeyword);
    connect(m_leftPane->friendList(), &FriendListWidget::selectedFriendChanged,
            this, [this](const QString& userId) {
                if (userId.isEmpty()) {
                    m_rightStack->setCurrentWidget(m_defaultPage);
                    return;
                }
                m_detailPage->setUserId(userId);
                m_rightStack->setCurrentWidget(m_detailPage);
            });
    connect(m_detailPage, &FriendDetailPage::friendDeleted,
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
    p.setBrush(Qt::white);
    p.drawRect(rect());
}
