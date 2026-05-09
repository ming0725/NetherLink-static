#include "ApplicationBarItem.h"
#include "ApplicationBar.h"
#include "shared/services/ImageService.h"
#include "shared/ui/StyledActionMenu.h"
#include "shared/theme/ThemeManager.h"
#include "app/state/CurrentUser.h"
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

ApplicationBar::ApplicationBar(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);

#ifdef Q_OS_MACOS
    setAttribute(Qt::WA_TranslucentBackground);
#endif

    setAvatarSource(CurrentUser::instance().getAvatarPath());
    connect(&CurrentUser::instance(), &CurrentUser::identityChanged, this, [this]() {
        setAvatarSource(CurrentUser::instance().getAvatarPath());
        update();
    });

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
    notItem->setDarkModeInversionEnabled(false);
    addItem(notItem);

    moreOptionsItem = new ApplicationBarItem(":/resources/icon/menu.png");
    moreOptionsItem->setPixmapScale(0.57);
    addBottomItem(moreOptionsItem);

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
    QColor windowColor = ThemeManager::instance().color(ThemeColor::PanelBackground);
    windowColor.setAlpha(128);
    painter.fillRect(rect(), windowColor);
#endif
    int w = width();
    if (selectedItem) {
        painter.save();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ThemeManager::instance().color(ThemeColor::AppBarItemSelectedBackground));
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

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        item->paint(painter);
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        item->paint(painter);
    }
}

void ApplicationBar::addItem(ApplicationBarItem* item)
{
    if (!item) {
        return;
    }

    topItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
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

    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, y, iconSize, iconSize));
        y += iconSize + spacing;
    }
    int yb = height() - marginBottom - iconSize;
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        int x = (w - iconSize) / 2;
        item->setRect(QRect(x, yb, iconSize, iconSize));
        yb -= (iconSize + spacing);
    }

    if (selectedItem && highlightAnim->state() != QAbstractAnimation::Running) {
        highlightPosY = selectedItem->y();
    }
}

void ApplicationBar::onItemClicked(ApplicationBarItem* item)
{
    if (!item || !topItems.contains(item) || selectedItem == item)
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
    if (!item) {
        return;
    }

    bottomItems.append(item);
    item->setParent(this);
    connect(item, &ApplicationBarItem::updateRequested, this, QOverload<>::of(&ApplicationBar::update));
    layoutItems();
}

void ApplicationBar::mouseMoveEvent(QMouseEvent* event)
{
    setHoveredItem(itemAtPosition(event->pos()));
    QWidget::mouseMoveEvent(event);
}

void ApplicationBar::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton) {
        ApplicationBarItem* item = itemAtPosition(event->pos());
        if (item == moreOptionsItem) {
            showMoreOptionsMenu();
        } else {
            onItemClicked(item);
        }
    }

    QWidget::mousePressEvent(event);
}

void ApplicationBar::leaveEvent(QEvent* event)
{
    setHoveredItem(nullptr);
    QWidget::leaveEvent(event);
}

ApplicationBarItem* ApplicationBar::itemAtPosition(const QPoint& pos) const
{
    for (int i = 0; i < topItems.size(); ++i) {
        auto* item = topItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    for (int i = 0; i < bottomItems.size(); ++i) {
        auto* item = bottomItems.at(i);
        if (item->contains(pos)) {
            return item;
        }
    }
    return nullptr;
}

void ApplicationBar::setHoveredItem(ApplicationBarItem* item)
{
    if (hoveredItem == item) {
        return;
    }

    if (hoveredItem) {
        hoveredItem->setHovered(false);
    }
    hoveredItem = item;
    if (hoveredItem) {
        hoveredItem->setHovered(true);
    }
}

void ApplicationBar::showMoreOptionsMenu()
{
    if (!moreOptionsItem) {
        return;
    }

    auto* menu = new StyledActionMenu(this);
    menu->setItemHoverColor(QColor(238, 238, 238));
    QAction* settingsAction = menu->addAction(QStringLiteral("设置"));
    menu->addAction(QStringLiteral("主题颜色"));
    menu->addAction(QStringLiteral("聊天记录管理"));
    menu->addSeparator();
    menu->addAction(QStringLiteral("退出账号"));

    connect(settingsAction, &QAction::triggered, this, &ApplicationBar::settingsRequested);

    connect(menu, &QMenu::aboutToHide, this, [this, menu]() {
        if (moreOptionsItem) {
            moreOptionsItem->setSelected(false);
        }
        menu->deleteLater();
    });

    moreOptionsItem->setSelected(true);
    const QRect itemRect = moreOptionsItem->rect();
    const int horizontalOffset = menu->isUsingNativeMenu() ? 10 : 6;
    const int verticalOffset = menu->isUsingNativeMenu() ? -22 : -46;
    const QPoint popupPos = mapToGlobal(
            QPoint(itemRect.right() + horizontalOffset,
                   itemRect.y() - itemRect.height() + verticalOffset));
    menu->popupWhenMouseReleased(popupPos);
}
