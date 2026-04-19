#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "UserRepository.h"
#include "CurrentUser.h"
#include <QPainter>
#include <QPainterPath>

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_TranslucentBackground);
#endif

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
    painter.fillRect(rect(), QColor(0xF2, 0xF2, 0xF2, 64));
    int w = width();
    if (selectedItem) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(QColor(0xD8, 0xD8, 0xD8));
        int x = (width() - iconSize) / 2;
        QRect r(x, highlightPosY, iconSize, iconSize);
        painter.drawRoundedRect(r, 10, 10);
        painter.restore();
    }

    if (!avatarPixmap.isNull()) {
        painter.save();
        int x = (w - avatarSize) / 2;
        int y = topInset + marginTop + spacing;
        QPainterPath circlePath;
        circlePath.addEllipse(x, y, avatarSize, avatarSize);
        painter.setClipPath(circlePath);
        painter.drawPixmap(x, y, avatarSize, avatarSize,
                           avatarPixmap.scaled(avatarSize, avatarSize,
                                               Qt::KeepAspectRatioByExpanding,
                                               Qt::SmoothTransformation));
        painter.restore();
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

void ApplicationBar::setTopInset(int inset)
{
    const int clampedInset = qMax(0, inset);
    if (topInset == clampedInset) {
        return;
    }

    topInset = clampedInset;
    layoutItems();
    update();
}


void ApplicationBar::layoutItems() {
    int y = topInset + marginTop + spacing + avatarSize + avatarAndItemDist;
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

    if (selectedItem && highlightAnim->state() != QAbstractAnimation::Running) {
        highlightPosY = selectedItem->y();
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
