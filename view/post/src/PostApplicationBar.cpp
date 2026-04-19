#include "PostApplicationBar.h"
#include <QPainter>
#include <QPainterPath>
#include <QVariantAnimation>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QTimer>
#include <QStackedWidget>

PostApplicationBar::PostApplicationBar(QWidget* parent)
        : QWidget(parent)
        , m_parent(parent)
{
    setAttribute(Qt::WA_TranslucentBackground);
    auto *shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(30);
    shadow->setOffset(0, 0);
    shadow->setColor(QColor(150, 150, 150, 220));
    setGraphicsEffect(shadow);
    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(300);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged,
            this, &PostApplicationBar::onHighlightValueChanged);

    initItems();
    int n = items.size();
    int totalW = 0;
    for (auto* it : items) {
        totalW += it->width();
    }
    int totalSpacing = spacing * (n - 1);
    int totalMargins = margin * 2;
    int w = totalW + totalSpacing + totalMargins;
    int h = itemHeight + margin * 2;
    resize(w, h);
    layoutItems();
    m_updateTimer = new QTimer(this);
    connect(m_updateTimer, &QTimer::timeout, this, &PostApplicationBar::updateBlurBackground);
    m_updateTimer->start(16);
    resizeEvent(nullptr);
    QTimer::singleShot(50, this, [this]() {
        int w = items[selectedItem->index]->width();
        highlightX = items[selectedItem->index]->x();
        selectedRect.setRect(highlightX,
                             height() - itemHeight - margin,
                             w, itemHeight);
    });
    preSize = parent->size();
}

void PostApplicationBar::initItems() {
    QStringList labels = {"首页", "关注", "发表", "消息", "我"};
    for (int i = 0; i < labels.size(); ++i) {
        auto* item = new TextBarItem(labels[i], i, this);
        item->setFixedHeight(itemHeight);
        connect(item, &TextBarItem::clicked,
                this, [this, i]() { onItemClicked(i); });
        items.append(item);
    }
    selectedItem = items[0];
}

void PostApplicationBar::resizeEvent(QResizeEvent*) {
    layoutItems();
}

QSize PostApplicationBar::minimumSizeHint() const {
    return sizeHint();
}

void PostApplicationBar::layoutItems() {
    int startX = margin;
    int y = height() - itemHeight - margin;

    for (int i = 0; i < items.size(); ++i) {
        int w = items[i]->width();    // 各自宽度
        items[i]->setGeometry(startX,
                              y,
                              w,
                              itemHeight);
        startX += w + spacing;
    }
}

void PostApplicationBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);

    // —— 画模糊背景 和 半透明白底 —— //
    if (!m_blurredBackground.isNull()) {
        QPainterPath clipPath;
        clipPath.addRoundedRect(rect(), 15, 15);
        painter.setClipPath(clipPath);
        painter.drawImage(rect(), m_blurredBackground);
    }
    painter.setBrush(QColor(255, 255, 255, 100));
    painter.drawRoundedRect(rect(), 15, 15);

    // —— 高亮选中区域 —— //
    painter.setBrush(QColor(192, 192, 192, 192));
    painter.setPen(Qt::NoPen);
    painter.drawRoundedRect(selectedRect, 10, 10);

    // —— 在外层圆角区域画描边 —— //
    QPen pen(QColor(0x0099ff));
    pen.setWidth(4);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    painter.drawRoundedRect(rect(), 15, 15);
}

void PostApplicationBar::onItemClicked(int index) {
    if (selectedItem->index == index) return;

    int startX = highlightX;
    int endX = items[index]->x();
    highlightAnim->stop();
    highlightAnim->setStartValue(startX);
    highlightAnim->setEndValue(endX);
    highlightAnim->start();
    selectedItem = items[index];
    emit pageClicked(index);
    update();
}

void PostApplicationBar::setCurrentIndex(int index)
{
    if (index >= 0 && index < items.size()) {
        onItemClicked(index);
    }
}

void PostApplicationBar::onHighlightValueChanged(const QVariant &value) {
    highlightX = value.toInt();
    int w = items[selectedItem->index]->width();

    selectedRect.setRect(highlightX,
                         height() - itemHeight - margin,
                         w, itemHeight);
    update();
}

QSize PostApplicationBar::sizeHint() const {
    int n = items.size();
    int totalW = 0;
    for (auto* it : items) {
        totalW += it->width();
    }
    int totalSpacing = spacing * (n - 1);
    int totalMargins = margin * 2;
    int w = totalW + totalSpacing + totalMargins;
    int h = itemHeight + margin * 2;
    return QSize(w, h);
}

void PostApplicationBar::updateBlurBackground() {
    if (!m_parent || !isVisible() || !isEnableBlur) return;

    // 获取父窗口
    QWidget* postApp = m_parent;
    if (m_parent->size() != preSize) {
        // 解决拖动窗口大小频闪问题，不要乱动这个代码
        preSize = m_parent->size();
        m_blurredBackground = QImage(size(), QImage::Format_ARGB32);
        m_blurredBackground.fill(Qt::transparent);
        return;
    }
    // 直接获取PostApplication的背景
    // 创建一个临时的QPixmap来保存父窗口的截图
    QPixmap appShot(postApp->size());
    appShot.fill(Qt::transparent);
    // 临时保存自己的可见性并隐藏自己
    bool wasVisible = isVisible();
    hide();
    // 渲染父窗口到pixmap
    postApp->render(&appShot);
    // 恢复可见性
    if (wasVisible) {
        show();
    }
    // 获取当前控件在父窗口中的相对位置
    QRect myRect = geometry();
    // 从父窗口截图中裁剪出我们位置下的背景部分
    QPixmap backgroundShot = appShot.copy(myRect);
    // 应用模糊效果
    QGraphicsScene scene;
    QGraphicsPixmapItem item;
    item.setPixmap(backgroundShot);
    auto blurEffect = new QGraphicsBlurEffect;
    blurEffect->setBlurRadius(10);
    item.setGraphicsEffect(blurEffect);
    scene.addItem(&item);
    m_blurredBackground = QImage(size(), QImage::Format_ARGB32);
    m_blurredBackground.fill(Qt::transparent);
    QPainter painter(&m_blurredBackground);
    scene.render(&painter, QRectF(), QRectF(0, 0, width(), height()));
    delete blurEffect;
}
