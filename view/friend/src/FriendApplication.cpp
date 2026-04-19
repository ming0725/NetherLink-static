#include "FriendApplication.h"
#include <QResizeEvent>
#include <QPainter>

FriendApplication::FriendApplication(QWidget* parent)
        : QWidget(parent)
{
    // 1) 左侧面板：内部已创建 TopSearchWidget + FriendListWidget
    m_leftPane    = new LeftPane(this);

    // 2) 右侧默认页
    m_defaultPage = new DefaultPage(this);

    // 3) 分割器：将左、右面板加入
    m_splitter    = new QSplitter(Qt::Horizontal, this);
    m_splitter->addWidget(m_leftPane);
    m_splitter->addWidget(m_defaultPage);
    m_splitter->setHandleWidth(0);               // >0，允许拖拽
    m_splitter->setChildrenCollapsible(false);
    m_splitter->setStretchFactor(0, 0);          // 左侧不自动扩展
    m_splitter->setStretchFactor(1, 1);          // 右侧占满剩余

    // （可选）设置初始左侧宽度
    m_splitter->setSizes({ 200, width() - 200 });
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);

    // 去除窗口自带标题栏，若需要无边框效果
    setWindowFlag(Qt::FramelessWindowHint);
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
