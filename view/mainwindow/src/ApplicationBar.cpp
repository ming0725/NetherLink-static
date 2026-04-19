#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "UserRepository.h"
#include "CurrentUser.h"
#include <QPainter>
#include <QPainterPath>
#include <QRandomGenerator>
#include <QGraphicsBlurEffect>
#include <QImage>

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
    noiseTexture = QImage(100, 100, QImage::Format_ARGB32);
    auto *rng = QRandomGenerator::global();  // 获取全局随机数生成器
    for (int x = 0; x < noiseTexture.width(); ++x) {
        for (int y = 0; y < noiseTexture.height(); ++y) {
            int gray = rng->bounded(56) + 200;   // 200~255
            int alpha = rng->bounded(51) + 30;   // 30~80
            noiseTexture.setPixelColor(x, y, QColor(gray, gray, gray, alpha));
        }
    }

    setAvatar(UserRepository::instance().getAvatar(CurrentUser::instance().getUserId()));

    auto msgItem = new ApplicationBarItem(
            QPixmap(":/resources/icon/unselected_message.png"),
            QPixmap(":/resources/icon/selected_message.png"));
    addItem(msgItem);
    auto friendItem = new ApplicationBarItem(
            QPixmap(":/resources/icon/friend_unselected.png"),
            QPixmap(":/resources/icon/friend_selected.png"));
    friendItem->setPixmapScale(0.62);
    addItem(friendItem);

    auto momentItem = new ApplicationBarItem(
            QPixmap(":/resources/icon/unselected_blazer.png"),
            QPixmap(":/resources/icon/blazer.png"));
    momentItem->setPixmapScale(0.68);
    addItem(momentItem);

    auto aiChat = new ApplicationBarItem(
            QPixmap(":/resources/icon/unselected_aichat.png"),
            QPixmap(":/resources/icon/aichat.png"));
    aiChat->setPixmapScale(0.77);
    addItem(aiChat);

    auto notItem = new ApplicationBarItem(
            QPixmap(":/resources/icon/unselected_nether.png"),
            QPixmap(":/resources/icon/nether.png"));
    notItem->setPixmapScale(0.72);
    addItem(notItem);

    auto menuItem = new ApplicationBarItem(QPixmap(":/resources/icon/menu.png"));
    menuItem->setPixmapScale(0.57);
    addBottomItem(menuItem);

    auto starItem = new ApplicationBarItem(QPixmap(":/resources/icon/bookmark.png"));
    starItem->setPixmapScale(0.57);
    addBottomItem(starItem);

    auto folderItem = new ApplicationBarItem(QPixmap(":/resources/icon/folder.png"));
    folderItem->setPixmapScale(0.57);
    addBottomItem(folderItem);

    if (!topItems.empty()) {
        selectedItem = topItems[0];
        topItems[0]->setSelected(true);
        highlightPosY = selectedItem->y();
    }
    highlightAnim = new QVariantAnimation(this);
    highlightAnim->setDuration(250);
    highlightAnim->setEasingCurve(QEasingCurve::OutCubic);
    connect(highlightAnim, &QVariantAnimation::valueChanged, this, [this](const QVariant &value) {
        highlightPosY = value.toInt();
        update();
    });
}

void ApplicationBar::resizeEvent(QResizeEvent*) {
    layoutItems();
}

void ApplicationBar::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHints(QPainter::Antialiasing |
                           QPainter::SmoothPixmapTransform);
    painter.fillRect(rect(), QColor(0xc4, 0xdd, 0xe6, 180));
    //painter.fillRect(rect(), QColor(0xD0, 0x50, 0x10, 120));
    int w = width();
    painter.save();
    painter.setOpacity(0.5);  // 调整噪声层透明度
    painter.drawTiledPixmap(rect(), QPixmap::fromImage(noiseTexture));
    if (selectedItem) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xc8, 0xcf, 0xcd, 192));
        int x = (width() - iconSize) / 2;
        QRect r(x, highlightPosY, iconSize, iconSize);
        painter.drawRoundedRect(r, 10, 10);
        painter.restore();
    }

    if (!avatarPixmap.isNull()) {
        painter.restore();
        int x = (w - avatarSize) / 2;
        int y = marginTop + spacing;
        QPainterPath circlePath;
        circlePath.addEllipse(x, y, avatarSize, avatarSize);
        painter.setClipPath(circlePath);
        painter.drawPixmap(x, y, avatarSize, avatarSize,
                           avatarPixmap.scaled(avatarSize, avatarSize,
                                               Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation));
        painter.setClipping(false);
    }
}

void ApplicationBar::addItem(ApplicationBarItem* item)
{
    topItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::itemClicked, this, &ApplicationBar::onItemClicked);
    layoutItems();
}

void ApplicationBar::setAvatar(QPixmap pix)
{
    if (!pix.isNull()) {
        avatarPixmap = pix;
    }
    layoutItems();
}


void ApplicationBar::layoutItems() {
    int y = marginTop + spacing + avatarSize + avatarAndItemDist;
    int w = width();

    for (auto* item : topItems) {
        int x = (w - iconSize) / 2;
        item->setGeometry(x, y, iconSize, iconSize);
        y += iconSize + spacing;
    }
    int yb = height() - marginBottom - iconSize;
    for (auto* item : bottomItems) {
        int x = (w - iconSize) / 2;
        item->setGeometry(x, yb, iconSize, iconSize);
        yb -= (iconSize + spacing);
    }
}

void ApplicationBar::onItemClicked(ApplicationBarItem* item)
{
    if (selectedItem == item)
        return;

    if (selectedItem) selectedItem->setSelected(false);
    item->setSelected(true);

    int startY = highlightPosY;
    int endY = item->y();
    highlightAnim->stop();
    highlightAnim->setStartValue(startY);
    highlightAnim->setEndValue(endY);
    highlightAnim->start();

    selectedItem = item;
    emit applicationClicked(item);
}

void ApplicationBar::addBottomItem(ApplicationBarItem* item)
{
    bottomItems.append(item);
    item->setParent(this);
    layoutItems();
}