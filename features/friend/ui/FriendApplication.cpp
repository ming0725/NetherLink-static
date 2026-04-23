#include "FriendApplication.h"
#include <QResizeEvent>
#include <QPainter>

FriendApplication::LeftPane::LeftPane(QWidget* parent)
        : QWidget(parent)
        , m_searchInput(new IconLineEdit(this))
        , m_addButton(new StatefulPushButton("+", this))
        , m_content(new FriendListWidget(this))
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
    m_content->setStyleSheet("border-width:0px;border-style:solid;");
}

void FriendApplication::LeftPane::resizeEvent(QResizeEvent* ev)
{
    QWidget::resizeEvent(ev);

    const int topMargin = 24;
    const int bottomMargin = 12;
    const int leftMargin = 14;
    const int rightMargin = 14;
    const int spacing = 5;
    const int controlHeight = 26;
    const int buttonX = width() - rightMargin - controlHeight;

    m_addButton->setGeometry(buttonX, topMargin, controlHeight, controlHeight);
    m_searchInput->setGeometry(leftMargin,
                               topMargin,
                               qMax(0, buttonX - spacing - leftMargin),
                               controlHeight);

    const int contentY = topMargin + controlHeight + bottomMargin;
    m_content->setGeometry(0, contentY, width(), qMax(0, height() - contentY));
}

FriendApplication::FriendApplication(QWidget* parent)
        : QWidget(parent)
{
    // 1) 左侧面板：内部手动布局搜索框、加号按钮和好友列表
    m_leftPane    = new LeftPane(this);

    // 2) 右侧默认页
    m_defaultPage = new DefaultPage(this);

    // 3) 分割器：将左、右面板加入
    m_splitter    = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_defaultPage);
    m_splitter->setHandleWidth(0);               // >0，允许拖拽
    m_splitter->setStyleSheet("QSplitter::handle { background: transparent; border: none; }");
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
