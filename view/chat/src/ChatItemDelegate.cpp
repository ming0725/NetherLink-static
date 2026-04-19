#include "ChatItemDelegate.h"
#include "ChatListModel.h"
#include "UserRepository.h"
#include "ChatArea.h"
#include "NotificationManager.h"
#include "CurrentUser.h"
#include <QPainter>
#include <QTextLayout>
#include <QTextOption>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QPainterPath>
#include <QMenu>

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
    painter->setRenderHint(QPainter::Antialiasing);

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
        // 计算气泡最大宽度（窗口宽度的70%）
        int maxBubbleWidth = option.rect.width() * 0.7;
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

            bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + 5);
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

        // 计算气泡区域
        int maxBubbleWidth = option.rect.width() * 0.7;
        QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, message->isFromMe());

        // 如果是群聊消息，需要考虑名字区域的偏移
        if (message->isInGroupChat()) {
            bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + 5);
        }

        // 创建气泡路径（用于精确点击检测）
        QPainterPath bubblePath;
        bubblePath.addRoundedRect(bubbleRect, BUBBLE_RADIUS, BUBBLE_RADIUS);

        if (bubblePath.contains(mouseEvent->pos())) {
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
                return true;
            }
        }
    }
    return QStyledItemDelegate::editorEvent(event, model, option, index);
}

void ChatItemDelegate::drawBubble(QPainter* painter, const QRect& rect,
                                 bool isFromMe, const ChatMessage* message, bool isSelected) const
{
    // 设置气泡颜色
    QColor bubbleColor;
    if (isFromMe) {
        bubbleColor = isSelected ? QColor(0, 120, 215) : QColor(0, 145, 255);
    } else {
        bubbleColor = isSelected ? QColor(220, 220, 220) : QColor(255, 255, 255);
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
    } else if (message->getType() == MessageType::Image) {
        const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
        drawImageMessage(painter, rect, imgMsg->getImage(), isFromMe);
    }
}


void ChatItemDelegate::drawAvatar(QPainter* painter, const QRect& rect,
                                  const QString& userID) const
{
    painter->drawPixmap(rect, UserRepository::instance().getAvatar(userID));
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
        textColor = isSelected ? Qt::black : Qt::black;
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
                                      const QPixmap& image, bool isFromMe) const
{
    if (image.isNull()) return;

    QRect imageRect = rect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING,
                                  -BUBBLE_PADDING, -BUBBLE_PADDING);
    QSize scaledSize = image.size().scaled(imageRect.size(), Qt::KeepAspectRatio);

    QRect targetRect(imageRect.x() + (imageRect.width() - scaledSize.width()) / 2,
                    imageRect.y() + (imageRect.height() - scaledSize.height()) / 2,
                    scaledSize.width(), scaledSize.height());

    painter->drawPixmap(targetRect, image);
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
    painter->setPen(QColor(0xB1B1B1));
    QRect nameRect = rect;
    nameRect.setWidth(fm.horizontalAdvance(elidedName));
    painter->drawText(nameRect, Qt::AlignLeft | Qt::AlignVCenter, elidedName);

    // 如果有特殊身份（群主或管理员），绘制身份标签
    GroupRole role = message->getRole();
    if (role != GroupRole::Member) {
        QString roleText = (role == GroupRole::Owner) ? "群主" : "管理员";
        QColor bgColor = (role == GroupRole::Owner) ? QColor(0xF5DDCB) : QColor(0xC2E1F5);
        QColor textColor = (role == GroupRole::Owner) ? QColor(0xFF9C00) : QColor(0x0066CC);

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
        QColor bgColor = (role == GroupRole::Owner) ? QColor(0xF5DDCB) : QColor(0xC2E1F5);
        QColor textColor = (role == GroupRole::Owner) ? QColor(0xFF9C00) : QColor(0x0099FF);

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
    painter->setPen(QColor(0xB1B1B1));
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
        int maxBubbleWidth = option.rect.width() * 0.7;

        // 使用更大的字体
        QFont messageFont = QApplication::font();
        messageFont.setPixelSize(14);
        QFontMetrics fm(messageFont);

        int height = AVATAR_SIZE + 2 * BUBBLE_MARGIN;
        int bubbleHeight = 0;

        // 如果是群聊消息，需要额外的空间显示群成员信息
        if (message->isInGroupChat()) {
            height += NAME_HEIGHT;  // 名字高度 + 5像素间距
        }

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
            if (message->isInGroupChat()) {
                bubbleHeight += NAME_HEIGHT;
            }
        } else if (message->getType() == MessageType::Image) {
            const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
            QPixmap image = imgMsg->getImage();
            if (!image.isNull()) {
                QSize scaledSize = image.size().scaled(maxBubbleWidth - 2 * BUBBLE_PADDING,
                                                     200, Qt::KeepAspectRatio);
                bubbleHeight = scaledSize.height() + 2 * BUBBLE_PADDING;
            }
            if (message->isInGroupChat()) {
                bubbleHeight += 20; // 群图片消息空隙补偿
            }
        }

        height = qMax(height, bubbleHeight + 2 * BUBBLE_MARGIN);
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
        QPixmap image = imgMsg->getImage();
        if (!image.isNull()) {
            QSize scaledSize = image.size().scaled(maxWidth - 2 * BUBBLE_PADDING,
                                                 200, Qt::KeepAspectRatio);
            bubbleWidth = scaledSize.width() + 2 * BUBBLE_PADDING;
            bubbleHeight = scaledSize.height() + 2 * BUBBLE_PADDING;
        }
    }

    bubbleWidth = qMin(bubbleWidth, maxWidth);
    int x = isFromMe ? contentRect.right() - bubbleWidth - AVATAR_SIZE - BUBBLE_MARGIN
                     : AVATAR_SIZE + BUBBLE_MARGIN + LEFT_MARGIN;

    return QRect(x, contentRect.y() + BUBBLE_MARGIN,
                bubbleWidth, bubbleHeight);
}

QRect ChatItemDelegate::calculateAvatarRect(const QRect& contentRect,
                                          bool isFromMe) const
{
    int x = isFromMe ? contentRect.right() - AVATAR_SIZE : LEFT_MARGIN;
    return QRect(x, contentRect.y() + BUBBLE_MARGIN, AVATAR_SIZE, AVATAR_SIZE);
}

void ChatItemDelegate::showContextMenu(const QPoint& pos, const QModelIndex& index,
                                     const ChatMessage* message) const
{
    TransparentMenu* menu = new TransparentMenu(qobject_cast<QWidget*>(parent()));

    // 添加复制选项
    if (message->getType() == MessageType::Text) {
        QAction* copyAction = menu->addAction("复制");
        connect(copyAction, &QAction::triggered, [message, index, model = const_cast<QAbstractItemModel*>(index.model())]() {
            const TextMessage* textMessage = static_cast<const TextMessage*>(message);
            QApplication::clipboard()->setText(textMessage->getText());
            // 取消选中状态
            NotificationManager::instance()
                    .showMessage("复制成功！", NotificationManager::Success, CurrentUser::instance().getMainWindow());
            model->setData(index, false, Qt::UserRole + 1);
        });
        menu->addSeparator();
    }

    // 添加删除选项
    QAction* deleteAction = menu->addAction("删除");
    connect(deleteAction, &QAction::triggered, [index, model = index.model()]() {
        if (ChatListModel* chatModel = qobject_cast<ChatListModel*>(const_cast<QAbstractItemModel*>(model))) {
            chatModel->removeMessage(index.row());
            NotificationManager::instance()
                    .showMessage("删除成功！", NotificationManager::Success, CurrentUser::instance().getMainWindow());
        }
    });

    // 显示菜单
    menu->popup(pos);

    // 菜单关闭后自动删除，并取消选中状态
    connect(menu, &QMenu::aboutToHide, [index, model = const_cast<QAbstractItemModel*>(index.model())]() {
        model->setData(index, false, Qt::UserRole + 1);
    });
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
    painter->setBrush(QColor(0xF6F6F6));  // 浅灰色背景
    painter->drawRoundedRect(timeRect, TIME_HEADER_RADIUS, TIME_HEADER_RADIUS);

    // 绘制文本
    painter->setPen(QColor(153, 153, 153));  // 灰色文字
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
