#include "ChatItemDelegate.h"
#include "features/chat/model/ChatListModel.h"
#include "features/friend/data/UserRepository.h"
#include "ChatArea.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QMenu>
#include <QKeyEvent>

namespace {

QColor groupRoleBackgroundColor(GroupRole role)
{
    if (role == GroupRole::Owner) {
        return QColor(0xF5, 0xDD, 0xCB);
    }
    if (role == GroupRole::Admin && !ThemeManager::instance().isDark()) {
        return QColor(0xC2, 0xE1, 0xF5);
    }
    return ThemeManager::instance().color(ThemeColor::PanelRaisedBackground);
}

void showCopyNotification(QObject* owner)
{
    QWidget* widget = qobject_cast<QWidget*>(owner);
    GlobalNotification::showSuccess(widget, QStringLiteral("复制成功"));
}

} // namespace

ChatItemDelegate::ChatItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

void ChatItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    QVariant data = index.data(Qt::UserRole);
    const ChatMessage* message = nullptr;
    const TimeHeader* timeHeader = nullptr;

    if (data.canConvert<TimeHeader*>()) {
        timeHeader = data.value<TimeHeader*>();
    } else if (data.canConvert<ChatMessage*>()) {
        message = data.value<ChatMessage*>();
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    if (timeHeader) {
        // 绘制时间标识
        QRect timeHeaderRect = QRect(
            0,                          // x
            option.rect.y() + 6,        // y（顶部留出6像素间距）
            option.rect.width(),        // width
            TIME_HEADER_HEIGHT          // height
        );
        drawTimeHeader(painter, timeHeaderRect, timeHeader->text);
    } else if (message) {
        const int maxBubbleWidth = calculateMaxBubbleWidth(option.rect);
        bool isFromMe = message->isFromMe();

        // 绘制头像
        QRect avatarRect = calculateAvatarRect(option.rect, isFromMe);
        drawAvatar(painter, avatarRect, message->getSenderId());

        // 计算气泡位置
        QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, isFromMe);

        // 如果是群聊消息，绘制群成员信息
        if (message->isInGroupChat()) {
            QRect groupInfoRect = bubbleRect;
            groupInfoRect.setHeight(NAME_HEIGHT);

            if (isFromMe) {
                drawGroupInfoForMe(painter, groupInfoRect, message);
            } else {
                drawGroupInfo(painter, groupInfoRect, message);
            }

            bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + GROUP_INFO_GAP);
        }
        // 绘制气泡
        drawBubble(painter, bubbleRect, isFromMe, message, message->getIsSelected());
    }
    painter->restore();
}

bool ChatItemDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index)
{
    if (event->type() == QEvent::MouseButtonPress) {
        QMouseEvent* mouseEvent = static_cast<QMouseEvent*>(event);

        // 检查是否是底部空白区域或时间标识
        if (ChatListModel* chatModel = qobject_cast<ChatListModel*>(model)) {
            if (chatModel->isBottomSpace(index.row()) || chatModel->isTimeHeader(index.row())) {
                chatModel->clearSelection();
                return true;
            }
        }

        const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
        if (!message) return false;

        if (bubbleHitTest(option, index, mouseEvent->pos())) {
            // 无论是左键还是右键点击，都设置选中状态
            model->setData(index, true, Qt::UserRole + 1);

            // 如果是右键点击，显示上下文菜单
            if (mouseEvent->button() == Qt::RightButton) {
                showContextMenu(mouseEvent->globalPosition().toPoint(), index, message);
            }
        } else {
            // 点击在气泡外部，清除选中状态
            model->setData(index, false, Qt::UserRole + 1);
        }
        return true;
    } else if (event->type() == QEvent::KeyPress) {
        QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
        if (keyEvent->matches(QKeySequence::Copy)) {
            const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
            if (message && message->getType() == MessageType::Text && message->getIsSelected()) {
                const TextMessage* textMessage = static_cast<const TextMessage*>(message);
                QApplication::clipboard()->setText(textMessage->getText());
                showCopyNotification(parent());
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

bool ChatItemDelegate::bubbleHitTest(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message) {
        return false;
    }

    const int maxBubbleWidth = calculateMaxBubbleWidth(option.rect);
    QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, message->isFromMe());
    if (message->isInGroupChat()) {
        bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + GROUP_INFO_GAP);
    }

    QPainterPath bubblePath;
    const qreal bubbleRadius = message->getType() == MessageType::Image ? IMAGE_RADIUS : BUBBLE_RADIUS;
    bubblePath.addRoundedRect(bubbleRect, bubbleRadius, bubbleRadius);
    return bubblePath.contains(viewportPos);
}

void ChatItemDelegate::drawBubble(QPainter* painter, const QRect& rect,
                                 bool isFromMe, const ChatMessage* message, bool isSelected) const
{
    if (message->getType() == MessageType::Image) {
        const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
        drawImageMessage(painter, rect, imgMsg->getImageSource(), isSelected);
        return;
    }

    QColor bubbleColor;
    const bool dark = ThemeManager::instance().isDark();
    if (isFromMe) {
        const QColor ownBubbleColor = dark
                ? QColor(0x1e, 0x86, 0xdf)
                : ThemeManager::instance().color(ThemeColor::Accent);
        bubbleColor = isSelected ? ownBubbleColor.darker(118)
                                 : ownBubbleColor;
    } else if (dark) {
        bubbleColor = isSelected ? QColor(0x25, 0x29, 0x30)
                                 : QColor(0x2d, 0x31, 0x38);
    } else {
        bubbleColor = isSelected ? QColor(0xea, 0xea, 0xea)
                                 : ThemeManager::instance().color(ThemeColor::PanelBackground);
    }
    painter->setBrush(bubbleColor);
    painter->setPen(Qt::NoPen);

    // 创建气泡路径
    QPainterPath path;
    path.addRoundedRect(rect, BUBBLE_RADIUS, BUBBLE_RADIUS);

    // 绘制气泡背景
    painter->drawPath(path);

    // 根据消息类型绘制内容
    if (message->getType() == MessageType::Text) {
        drawTextMessage(painter, rect, message->getContent(), isFromMe, isSelected);
    }
}


void ChatItemDelegate::drawAvatar(QPainter* painter, const QRect& rect,
                                  const QString& userID) const
{
    const QString avatarPath = UserRepository::instance().requestUserAvatarPath(userID);
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   rect.width(),
                                                                   painter->device()->devicePixelRatioF());
    painter->drawPixmap(rect, avatar);
}

void ChatItemDelegate::drawTextMessage(QPainter* painter, const QRect& rect,
                                     const QString& text, bool isFromMe, bool isSelected) const
{
    QRect textRect = rect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING,
                                 -BUBBLE_PADDING, -BUBBLE_PADDING);

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setAlignment(Qt::AlignLeft | Qt::AlignVCenter);

    QColor textColor;
    if (isFromMe) {
        textColor = isSelected ? Qt::white : Qt::white;
    } else {
        textColor = ThemeManager::instance().color(ThemeColor::PrimaryText);
    }
    painter->setPen(textColor);

    // 设置更大的字体
    QFont messageFont = QApplication::font();
    messageFont.setPixelSize(14);  // 设置字体大小为14像素
    painter->setFont(messageFont);

    QTextLayout textLayout(text, messageFont);
    textLayout.setTextOption(option);

    // 计算文本布局
    qreal height = 0;
    textLayout.beginLayout();
    forever {
        QTextLine line = textLayout.createLine();
        if (!line.isValid())
            break;

        line.setLineWidth(textRect.width());
        line.setPosition(QPointF(0, height));
        height += line.height();
    }
    textLayout.endLayout();

    // 绘制文本
    painter->save();
    painter->translate(textRect.left(), textRect.top());
    textLayout.draw(painter, QPointF(0, 0));
    painter->restore();
}

void ChatItemDelegate::drawImageMessage(QPainter* painter, const QRect& rect,
                                      const QString& imageSource,
                                      bool isSelected) const
{
    const QSize sourceSize = ImageService::instance().sourceSize(imageSource);
    if (!sourceSize.isValid()) {
        return;
    }

    const QPixmap image = ImageService::instance().scaled(imageSource,
                                                          rect.size(),
                                                          Qt::KeepAspectRatio,
                                                          painter->device()->devicePixelRatioF());

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, IMAGE_RADIUS, IMAGE_RADIUS);
    painter->save();
    painter->setClipPath(clipPath);
    painter->drawPixmap(rect, image);
    if (isSelected) {
        painter->fillRect(rect, QColor(128, 128, 128, 80));
    }
    painter->restore();
}

void ChatItemDelegate::drawGroupInfo(QPainter* painter, const QRect& rect,
                                   const ChatMessage* message) const
{
    painter->save();

    // 设置字体
    QFont nameFont = QApplication::font();
    nameFont.setPixelSize(12);
    painter->setFont(nameFont);

    // 获取成员名字
    QString name = message->getSenderName();
    if (name.isEmpty()) {
        name = message->getSenderId();
    }

    // 限制名字长度
    QFontMetrics fm(nameFont);
    QString elidedName = fm.elidedText(name, Qt::ElideRight, NAME_MAX_WIDTH);

    // 绘制名字（灰色）
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    QRect nameRect = rect;
    nameRect.setWidth(fm.horizontalAdvance(elidedName));
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    // 如果有特殊身份（群主或管理员），绘制身份标签
    GroupRole role = message->getRole();
    if (role != GroupRole::Member) {
        QString roleText = (role == GroupRole::Owner) ? "群主" : "管理员";
        QColor bgColor = groupRoleBackgroundColor(role);
        QColor textColor = (role == GroupRole::Owner) ? QColor(0xFF9C00) : ThemeManager::instance().color(ThemeColor::Accent);

        // 计算身份标签的宽度和位置
        QRect roleRect = nameRect;
        roleRect.setLeft(nameRect.right() + 5);  // 与名字保持5像素间距
        roleRect.setWidth(fm.horizontalAdvance(roleText) + 2 * ROLE_PADDING);
        roleRect.setHeight(ROLE_HEIGHT);
        roleRect.moveTop(nameRect.top() + (nameRect.height() - ROLE_HEIGHT) / 2);

        // 绘制身份标签背景
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawRoundedRect(roleRect, ROLE_RADIUS, ROLE_RADIUS);

        // 绘制身份标签文字
        painter->setPen(textColor);
        painter->drawText(roleRect, Qt::AlignCenter, roleText);
    }

    painter->restore();
}

void ChatItemDelegate::drawGroupInfoForMe(QPainter* painter, const QRect& rect,
                                        const ChatMessage* message) const
{
    painter->save();

    // 设置字体
    QFont nameFont = QApplication::font();
    nameFont.setPixelSize(12);
    painter->setFont(nameFont);
    QFontMetrics fm(nameFont);

    // 获取成员名字
    QString name = message->getSenderName();
    if (name.isEmpty()) {
        name = message->getSenderId();
    }
    QString elidedName = fm.elidedText(name, Qt::ElideRight, NAME_MAX_WIDTH);
    // 计算总宽度，确保右对齐到头像位置
    int totalWidth = 0;
    QString roleText;
    int roleWidth = 0;

    // 如果有特殊身份，计算身份标签宽度
    GroupRole role = message->getRole();
    if (role != GroupRole::Member) {
        roleText = (role == GroupRole::Owner) ? "群主" : "管理员";
        roleWidth = fm.horizontalAdvance(roleText) + 2 * ROLE_PADDING;
        totalWidth += roleWidth + 5;  // 5像素间距
    }

    // 计算名字宽度
    int nameWidth = qMin(fm.horizontalAdvance(elidedName), NAME_MAX_WIDTH);
    totalWidth += nameWidth;

    // 计算起始位置（右对齐到头像位置）
    int startX = rect.right() - totalWidth;

    // 如果有特殊身份，先绘制身份标签（在左边）
    if (role != GroupRole::Member) {
        QColor bgColor = groupRoleBackgroundColor(role);
        QColor textColor = (role == GroupRole::Owner) ? QColor(0xFF9C00) : ThemeManager::instance().color(ThemeColor::Accent);

        QRect roleRect(startX, rect.top() + (rect.height() - ROLE_HEIGHT) / 2,
                      roleWidth, ROLE_HEIGHT);

        // 绘制身份标签背景
        painter->setPen(Qt::NoPen);
        painter->setBrush(bgColor);
        painter->drawRoundedRect(roleRect, ROLE_RADIUS, ROLE_RADIUS);

        // 绘制身份标签文字
        painter->setPen(textColor);
        painter->drawText(roleRect, Qt::AlignCenter, roleText);

        startX += roleWidth + 5;  // 移动到名字起始位置
    }

    // 绘制名字（灰色）
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    QRect nameRect(startX, rect.top(), nameWidth, rect.height());

    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    painter->restore();
}

QSize ChatItemDelegate::sizeHint(const QStyleOptionViewItem& option,
                                const QModelIndex& index) const
{
    QVariant data = index.data(Qt::UserRole);
    const ChatMessage* message = nullptr;
    const TimeHeader* timeHeader = nullptr;

    if (data.canConvert<TimeHeader*>()) {
        timeHeader = data.value<TimeHeader*>();
    } else if (data.canConvert<ChatMessage*>()) {
        message = data.value<ChatMessage*>();
    } else if (data.canConvert<int>()) {
        // 处理底部空白项
        int height = data.toInt();
        return QSize(option.rect.width(), height);
    }

    if (timeHeader) {
        // 时间标识的高度（包括上下间距）
        return QSize(option.rect.width(), TIME_HEADER_HEIGHT + 12);  // 12是上下各6像素的间距
    } else if (message) {
        const int maxBubbleWidth = calculateMaxBubbleWidth(option.rect);

        // 使用更大的字体
        QFont messageFont = QApplication::font();
        messageFont.setPixelSize(14);
        QFontMetrics fm(messageFont);

        int bubbleHeight = 0;
        const int groupInfoHeight = message->isInGroupChat() ? (NAME_HEIGHT + GROUP_INFO_GAP) : 0;

        if (message->getType() == MessageType::Text) {
            QString text = message->getContent();

            QTextLayout textLayout(text, messageFont);
            QTextOption textOption;
            textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
            textLayout.setTextOption(textOption);

            qreal textHeight = 0;
            textLayout.beginLayout();
            forever {
                QTextLine line = textLayout.createLine();
                if (!line.isValid())
                    break;

                line.setLineWidth(maxBubbleWidth - 2 * BUBBLE_PADDING);
                line.setPosition(QPointF(0, textHeight));
                textHeight += line.height();
            }
            textLayout.endLayout();

            bubbleHeight = qCeil(textHeight) + 2 * BUBBLE_PADDING;
        } else if (message->getType() == MessageType::Image) {
            const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
            const QSize sourceSize = ImageService::instance().sourceSize(imgMsg->getImageSource());
            if (sourceSize.isValid()) {
                QSize scaledSize = sourceSize.scaled(maxBubbleWidth,
                                                     IMAGE_MAX_HEIGHT,
                                                     Qt::KeepAspectRatio);
                bubbleHeight = scaledSize.height();
            }
        }

        const int contentHeight = qMax(AVATAR_SIZE, groupInfoHeight + bubbleHeight);
        const int height = contentHeight + 2 * BUBBLE_MARGIN;
        return QSize(option.rect.width(), height);
    }

    return QSize(0, 0);
}

QRect ChatItemDelegate::calculateBubbleRect(const QRect& contentRect,
                                          const ChatMessage* message,
                                          int maxWidth, bool isFromMe) const
{
    // 使用更大的字体
    QFont messageFont = QApplication::font();
    messageFont.setPixelSize(14);
    QFontMetrics fm(messageFont);

    int bubbleWidth = 0;
    int bubbleHeight = 0;

    if (message->getType() == MessageType::Text) {
        // 使用QTextLayout计算实际文本高度
        QString text = message->getContent();
        QTextLayout textLayout(text, messageFont);
        QTextOption textOption;
        textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
        textLayout.setTextOption(textOption);

        qreal textHeight = 0;
        qreal textWidth = 0;
        textLayout.beginLayout();
        forever {
            QTextLine line = textLayout.createLine();
            if (!line.isValid())
                break;

            line.setLineWidth(maxWidth - 2 * BUBBLE_PADDING);
            line.setPosition(QPointF(0, textHeight));
            textHeight += line.height();
            textWidth = qMax(textWidth, line.naturalTextWidth());
        }
        textLayout.endLayout();

        bubbleWidth = qCeil(textWidth) + 2 * BUBBLE_PADDING;
        bubbleHeight = qCeil(textHeight) + 2 * BUBBLE_PADDING;
    } else if (message->getType() == MessageType::Image) {
        const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
        const QSize sourceSize = ImageService::instance().sourceSize(imgMsg->getImageSource());
        if (sourceSize.isValid()) {
            QSize scaledSize = sourceSize.scaled(maxWidth,
                                                 IMAGE_MAX_HEIGHT,
                                                 Qt::KeepAspectRatio);
            bubbleWidth = scaledSize.width();
            bubbleHeight = scaledSize.height();
        }
    }

    bubbleWidth = qMin(bubbleWidth, maxWidth);
    const int x = isFromMe
            ? contentRect.right() - HORIZONTAL_EDGE_MARGIN - AVATAR_SIZE - BUBBLE_MARGIN - bubbleWidth + 1
            : HORIZONTAL_EDGE_MARGIN + AVATAR_SIZE + BUBBLE_MARGIN;

    return QRect(x, contentRect.y() + BUBBLE_MARGIN,
                bubbleWidth, bubbleHeight);
}

QRect ChatItemDelegate::calculateAvatarRect(const QRect& contentRect,
                                          bool isFromMe) const
{
    const int x = isFromMe
            ? contentRect.right() - HORIZONTAL_EDGE_MARGIN - AVATAR_SIZE + 1
            : HORIZONTAL_EDGE_MARGIN;
    return QRect(x, contentRect.y() + BUBBLE_MARGIN, AVATAR_SIZE, AVATAR_SIZE);
}

int ChatItemDelegate::calculateMaxBubbleWidth(const QRect& contentRect) const
{
    const int availableWidth = contentRect.width()
            - 2 * HORIZONTAL_EDGE_MARGIN
            - AVATAR_SIZE
            - BUBBLE_MARGIN
            - 12;
    return qMax(120, qMin(static_cast<int>(contentRect.width() * 0.7), availableWidth));
}

void ChatItemDelegate::showContextMenu(const QPoint& pos, const QModelIndex& index,
                                     const ChatMessage* message) const
{
    StyledActionMenu* menu = new StyledActionMenu(qobject_cast<QWidget*>(parent()));
    const QPersistentModelIndex persistentIndex(index);

    // 添加复制选项
    if (message->getType() == MessageType::Text) {
        QAction* copyAction = menu->addAction("复制");
        connect(copyAction, &QAction::triggered, [this, message, index, model = const_cast<QAbstractItemModel*>(index.model())]() {
            const TextMessage* textMessage = static_cast<const TextMessage*>(message);
            QApplication::clipboard()->setText(textMessage->getText());
            showCopyNotification(parent());
            model->setData(index, false, Qt::UserRole + 1);
        });
        menu->addSeparator();
    }

    // 添加删除选项
    QAction* deleteAction = menu->addAction("删除");
    StyledActionMenu::setActionColors(deleteAction,
                                      QColor(235, 87, 87),
                                      QColor(255, 255, 255),
                                      QColor(235, 87, 87));
    connect(deleteAction, &QAction::triggered, [this, index, model = index.model()]() {
        Q_UNUSED(model);
        emit const_cast<ChatItemDelegate*>(this)->deleteRequested(index.row());
    });

    // 菜单关闭后自动删除，并取消选中状态
    connect(menu, &QMenu::aboutToHide, menu, [menu, persistentIndex, model = const_cast<QAbstractItemModel*>(index.model())]() {
        if (persistentIndex.isValid()) {
            model->setData(persistentIndex, false, Qt::UserRole + 1);
        }
        menu->deleteLater();
    });

    // 原生 NSMenu 不能在 MouseButtonPress 的同步处理里立刻进入 tracking，
    // 否则 Qt 可能丢失对应 release 并影响其它区域的事件状态。
    menu->popupWhenMouseReleased(pos);
}

void ChatItemDelegate::drawTimeHeader(QPainter* painter, const QRect& rect,
                                    const QString& text) const
{
    // 设置字体
    QFont font = QApplication::font();
    font.setPixelSize(TIME_HEADER_FONT_SIZE);
    painter->setFont(font);

    // 计算文本宽度
    QFontMetrics fm(font);
    int textWidth = fm.horizontalAdvance(text);

    // 计算时间标识的矩形区域（居中）
    int totalWidth = textWidth + 2 * TIME_HEADER_PADDING;
    totalWidth = qMax(totalWidth, TIME_HEADER_MIN_WIDTH);

    QRect timeRect(
        (rect.width() - totalWidth) / 2,  // 水平居中
        rect.y(),                         // 使用传入的y坐标
        totalWidth,
        TIME_HEADER_HEIGHT
    );

    // 绘制背景
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::PanelRaisedBackground));
    painter->drawRoundedRect(timeRect, TIME_HEADER_RADIUS, TIME_HEADER_RADIUS);

    // 绘制文本
    painter->setPen(ThemeManager::instance().color(ThemeColor::TertiaryText));
    painter->drawText(timeRect, Qt::AlignCenter, text);
}

QRect ChatItemDelegate::calculateTimeHeaderRect(const QRect& contentRect,
                                              const QString& text) const
{
    QFont font = QApplication::font();
    font.setPixelSize(TIME_HEADER_FONT_SIZE);
    QFontMetrics fm(font);

    // 计算文本宽度，确保最小宽度
    int textWidth = qMax(fm.horizontalAdvance(text) + 2 * TIME_HEADER_PADDING,
                        TIME_HEADER_MIN_WIDTH);

    // 居中放置
    int x = (contentRect.width() - textWidth) / 2;

    // 垂直方向上居中，但考虑到时间标识的实际高度
    int y = (contentRect.height() - TIME_HEADER_HEIGHT) / 2;

    return QRect(x, y, textWidth, TIME_HEADER_HEIGHT);
}
