#include "AiChatApplication.h"
#include <QPainter>

AiChatApplication::AiChatApplication(QWidget* parent)
    : QWidget(parent)
{
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
    m_splitter->setSizes({ 220, width() - 220 });
    m_splitter->handle(1)->setCursor(Qt::SizeHorCursor);
}

void AiChatApplication::resizeEvent(QResizeEvent *event) {
    m_splitter->setGeometry(0, 0, width(), height());
}

void AiChatApplication::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(255, 255, 255, 255));
}

