#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "shared/services/ImageService.h"
#include "features/friend/data/UserRepository.h"
#include "app/state/CurrentUser.h"
#include <QPainter>
#include <QPainterPath>

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    setAvatarSource(UserRepository::instance().requestUserAvatarPath(CurrentUser::instance().getUserId()));

    auto msgItem = new ApplicationBarItem(
            ":/resources/icon/unselected_message.png",
            ":/resources/icon/selected_message.png");
    addItem(msgItem);
    auto friendItem = new ApplicationBarItem(
            ":/resources/icon/friend_unselected.png",
            ":/resources/icon/friend_selected.png");
    friendItem->setPixmapScale(0.62);
    addItem(friendItem);

    auto momentItem = new ApplicationBarItem(
            ":/resources/icon/unselected_blazer.png",
            ":/resources/icon/blazer.png");
    momentItem->setPixmapScale(0.68);
    addItem(momentItem);

    auto aiChat = new ApplicationBarItem(
            ":/resources/icon/unselected_aichat.png",
            ":/resources/icon/aichat.png");
    aiChat->setPixmapScale(0.77);
    addItem(aiChat);

    auto notItem = new ApplicationBarItem(
            ":/resources/icon/unselected_nether.png",
            ":/resources/icon/nether.png");
    notItem->setPixmapScale(0.72);
    addItem(notItem);

    auto menuItem = new ApplicationBarItem(":/resources/icon/menu.png");
    menuItem->setPixmapScale(0.57);
    addBottomItem(menuItem);

    auto starItem = new ApplicationBarItem(":/resources/icon/bookmark.png");
    starItem->setPixmapScale(0.57);
    addBottomItem(starItem);

    auto folderItem = new ApplicationBarItem(":/resources/icon/folder.png");
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
#ifdef Q_OS_WIN
    painter.fillRect(rect(), QColor(0xFF, 0xFF, 0xFF, 128));
#endif
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

    if (!avatarSource.isEmpty()) {
        painter.save();
        int x = (w - avatarSize) / 2;
        int y = topInset + marginTop + spacing;
        const QPixmap avatarPixmap = ImageService::instance().circularAvatar(avatarSource,
                                                                             avatarSize,
                                                                             painter.device()->devicePixelRatioF());
        painter.drawPixmap(x, y, avatarSize, avatarSize, avatarPixmap);
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

void ApplicationBar::setAvatarSource(const QString& source)
{
    avatarSource = source;
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

void ApplicationBar::setCurrentTopIndex(int index)
{
    if (index < 0 || index >= topItems.size()) {
        return;
    }

    onItemClicked(topItems.at(index));
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
