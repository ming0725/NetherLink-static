#include "ChatItemDelegate.h"
#include "app/state/CurrentUser.h"
#include "features/chat/model/ChatListModel.h"
#include "features/friend/data/UserRepository.h"
#include "ChatArea.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextOption>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QMenu>
#include <QKeyEvent>
#include <QtMath>

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

QColor linkTextColor(bool dark, bool isFromMe)
{
    if (isFromMe) {
        return QColor(0xbf, 0xe9, 0xff);
    }

    return dark ? QColor(0x6a, 0xb7, 0xff) : QColor(0x1b, 0x6e, 0xd6);
}

QString documentTextForLayout(QString text)
{
    text.replace(QLatin1Char('\n'), QChar::LineSeparator);
    return text;
}

QString textLayoutCacheKey(const QString& text, const QFont& font, int textWidth)
{
    QString key = font.toString();
    key.reserve(key.size() + text.size() + 24);
    key += QLatin1Char('\x1f');
    key += QString::number(qMax(1, textWidth));
    key += QLatin1Char('\x1f');
    key += text;
    return key;
}

QString textDocumentCacheKey(const QString& text,
                             const QFont& font,
                             int textWidth,
                             const QColor& textColor,
                             bool isFromMe,
                             bool dark)
{
    QString key = textLayoutCacheKey(text, font, textWidth);
    key += QLatin1Char('\x1f');
    key += QString::number(textColor.rgba());
    key += QLatin1Char('\x1f');
    key += isFromMe ? QLatin1Char('1') : QLatin1Char('0');
    key += QLatin1Char('\x1f');
    key += dark ? QLatin1Char('1') : QLatin1Char('0');
    return key;
}

int textCacheCost(const QString& text)
{
    return qBound(1, text.size() / 512 + 1, 16);
}

} // namespace

ChatItemDelegate::ChatItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    m_textDocumentCache.setMaxCost(96);
    m_textSizeCache.setMaxCost(256);
    m_urlRangesCache.setMaxCost(256);
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
        drawBubble(painter, bubbleRect, isFromMe, message, message->getIsSelected(), index);
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

int ChatItemDelegate::characterIndexAt(const QStyleOptionViewItem& option,
                                       const QModelIndex& index,
                                       const QPoint& viewportPos,
                                       bool allowLineWhitespace) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return -1;
    }

    const QString text = message->getContent();
    if (text.isEmpty()) {
        return -1;
    }

    const int maxBubbleWidth = calculateMaxBubbleWidth(option.rect);
    QRect bubbleRect = calculateBubbleRect(option.rect, message, maxBubbleWidth, message->isFromMe());
    if (message->isInGroupChat()) {
        bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + GROUP_INFO_GAP);
    }

    const QRect textRect = calculateTextRect(bubbleRect);
    const QRect hitRect = allowLineWhitespace
            ? bubbleRect.adjusted(-2, -2, 2, 2)
            : textRect;
    if (!hitRect.contains(viewportPos)) {
        return -1;
    }

    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = message->isFromMe()
            ? Qt::white
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           textRect.width(),
                                                           textColor,
                                                           message->isFromMe(),
                                                           dark);

    const QPointF local = viewportPos - textRect.topLeft();
    const qreal height = textDocument.size().height();

    if (local.y() < 0) {
        return allowLineWhitespace ? 0 : -1;
    }
    if (local.y() == 0) {
        return 0;
    }
    if (local.y() >= height) {
        return allowLineWhitespace ? text.size() : -1;
    }

    const int lineCursor = SelectableText::lineCursorAt(textDocument,
                                                            local,
                                                            text.size(),
                                                            allowLineWhitespace);
    if (lineCursor >= 0) {
        return lineCursor;
    }

    const Qt::HitTestAccuracy accuracy = allowLineWhitespace ? Qt::FuzzyHit : Qt::ExactHit;
    const int cursor = textDocument.documentLayout()->hitTest(local, accuracy);
    if (cursor < 0) {
        return SelectableText::lineCursorAt(textDocument,
                                                local,
                                                text.size(),
                                                allowLineWhitespace);
    }

    return qBound(0, cursor, text.size());
}

QString ChatItemDelegate::urlAt(const QStyleOptionViewItem& option,
                                const QModelIndex& index,
                                const QPoint& viewportPos) const
{
    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return {};
    }

    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return {};
    }

    const QVector<TextRange> urls = cachedUrlRanges(message->getContent());
    for (const TextRange& url : urls) {
        const int end = url.start + url.length;
        if (cursor >= url.start && cursor <= end) {
            return url.text;
        }
    }

    return {};
}

bool ChatItemDelegate::selectWordAt(const QStyleOptionViewItem& option,
                                    const QModelIndex& index,
                                    const QPoint& viewportPos)
{
    const int cursor = characterIndexAt(option, index, viewportPos);
    if (cursor < 0) {
        return false;
    }

    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return false;
    }

    const SelectableText::Range range = SelectableText::wordRangeAt(message->getContent(), cursor);
    if (!range.isValid()) {
        return false;
    }

    setSelection(index, range.start, range.end);
    return hasSelection();
}

void ChatItemDelegate::setSelection(const QModelIndex& index, int anchor, int cursor)
{
    m_selectionIndex = QPersistentModelIndex(index);
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    const int textLength = message && message->getType() == MessageType::Text
            ? message->getContent().size()
            : 0;
    m_selection.set(anchor, cursor, textLength);
}

void ChatItemDelegate::clearSelection()
{
    m_selectionIndex = QPersistentModelIndex();
    m_selection.clear();
}

bool ChatItemDelegate::hasSelection() const
{
    return m_selectionIndex.isValid() && m_selection.hasSelection();
}

bool ChatItemDelegate::selectionContains(const QModelIndex& index, int cursor) const
{
    if (!hasSelection() || m_selectionIndex != index || cursor < 0) {
        return false;
    }

    return m_selection.contains(cursor);
}

QString ChatItemDelegate::selectedText() const
{
    if (!hasSelection()) {
        return {};
    }

    const ChatMessage* message = m_selectionIndex.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Text) {
        return {};
    }

    const QString text = message->getContent();
    return m_selection.selectedText(text);
}

QPersistentModelIndex ChatItemDelegate::selectionIndex() const
{
    return m_selectionIndex;
}

void ChatItemDelegate::drawBubble(QPainter* painter, const QRect& rect,
                                  bool isFromMe, const ChatMessage* message, bool isSelected,
                                  const QModelIndex& index) const
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
        drawTextMessage(painter, rect, message->getContent(), isFromMe, index);
    }
}


void ChatItemDelegate::drawAvatar(QPainter* painter, const QRect& rect,
                                  const QString& userID) const
{
    const CurrentUser& currentUser = CurrentUser::instance();
    const QString avatarPath = currentUser.isCurrentUserId(userID)
            ? currentUser.getAvatarPath()
            : UserRepository::instance().requestUserAvatarPath(userID);
    const QPixmap avatar = ImageService::instance().circularAvatar(avatarPath,
                                                                   rect.width(),
                                                                   painter->device()->devicePixelRatioF());
    painter->drawPixmap(rect, avatar);
}

void ChatItemDelegate::drawTextMessage(QPainter* painter, const QRect& rect,
                                       const QString& text, bool isFromMe,
                                       const QModelIndex& index) const
{
    const QRect textRect = calculateTextRect(rect);
    const bool dark = ThemeManager::instance().isDark();
    const QColor textColor = isFromMe
            ? Qt::white
            : ThemeManager::instance().color(ThemeColor::PrimaryText);
    const QColor selectionColor = isFromMe
            ? QColor(255, 255, 255, 88)
            : (dark ? QColor(0x4c, 0x82, 0xc5, 150) : QColor(0x8b, 0xb7, 0xff, 130));

    const QTextDocument& textDocument = cachedTextDocument(text,
                                                           messageFont(),
                                                           textRect.width(),
                                                           textColor,
                                                           isFromMe,
                                                           dark);

    QAbstractTextDocumentLayout::PaintContext paintContext;
    if (m_selectionIndex == index && hasSelection()) {
        QTextCharFormat selectionFormat;
        selectionFormat.setBackground(selectionColor);
        selectionFormat.setForeground(textColor);

        QAbstractTextDocumentLayout::Selection selection;
        selection.cursor = QTextCursor(const_cast<QTextDocument*>(&textDocument));
        selection.cursor.setPosition(m_selection.start());
        selection.cursor.setPosition(m_selection.end(), QTextCursor::KeepAnchor);
        selection.format = selectionFormat;
        paintContext.selections.push_back(selection);
    }

    painter->save();
    painter->translate(textRect.left(), textRect.top());
    textDocument.documentLayout()->draw(painter, paintContext);
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

        int bubbleHeight = 0;
        const int groupInfoHeight = message->isInGroupChat() ? (NAME_HEIGHT + GROUP_INFO_GAP) : 0;

        if (message->getType() == MessageType::Text) {
            const QSize textSize = textDocumentSize(message->getContent(),
                                                    messageFont(),
                                                    maxBubbleWidth - 2 * BUBBLE_PADDING);
            bubbleHeight = textSize.height() + 2 * BUBBLE_PADDING;
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
    int bubbleWidth = 0;
    int bubbleHeight = 0;

    if (message->getType() == MessageType::Text) {
        const QSize textSize = textDocumentSize(message->getContent(),
                                                messageFont(),
                                                maxWidth - 2 * BUBBLE_PADDING);
        bubbleWidth = textSize.width() + 2 * BUBBLE_PADDING;
        bubbleHeight = textSize.height() + 2 * BUBBLE_PADDING;
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

QRect ChatItemDelegate::calculateTextRect(const QRect& bubbleRect) const
{
    return bubbleRect.adjusted(BUBBLE_PADDING, BUBBLE_PADDING,
                               -BUBBLE_PADDING, -BUBBLE_PADDING);
}

QFont ChatItemDelegate::messageFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(14);
    return font;
}

QSize ChatItemDelegate::textDocumentSize(const QString& text,
                                         const QFont& font,
                                         int maxTextWidth) const
{
    const QString key = textLayoutCacheKey(text, font, maxTextWidth);
    if (const TextSizeCacheEntry* entry = m_textSizeCache.object(key)) {
        return entry->size;
    }

    QTextDocument textDocument;
    textDocument.setUndoRedoEnabled(false);
    textDocument.setDocumentMargin(0);
    textDocument.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    textDocument.setDefaultTextOption(textOption);
    textDocument.setPlainText(documentTextForLayout(text));
    textDocument.setTextWidth(qMax(1, maxTextWidth));

    const int width = qMin(qCeil(textDocument.idealWidth()), maxTextWidth);
    const int height = qCeil(textDocument.size().height());
    const QSize size(qCeil(width), qCeil(height));

    auto* entry = new TextSizeCacheEntry;
    entry->size = size;
    m_textSizeCache.insert(key, entry, textCacheCost(text));
    return size;
}

const QTextDocument& ChatItemDelegate::cachedTextDocument(const QString& text,
                                                          const QFont& font,
                                                          int textWidth,
                                                          const QColor& textColor,
                                                          bool isFromMe,
                                                          bool dark) const
{
    const QString key = textDocumentCacheKey(text, font, textWidth, textColor, isFromMe, dark);
    if (const TextDocumentCacheEntry* entry = m_textDocumentCache.object(key)) {
        return entry->document;
    }

    auto* entry = new TextDocumentCacheEntry;
    configureTextDocument(entry->document,
                          text,
                          font,
                          textWidth,
                          textColor,
                          isFromMe,
                          dark);
    m_textDocumentCache.insert(key, entry, textCacheCost(text));
    return m_textDocumentCache.object(key)->document;
}

QVector<ChatItemDelegate::TextRange> ChatItemDelegate::cachedUrlRanges(const QString& text) const
{
    if (const UrlRangesCacheEntry* entry = m_urlRangesCache.object(text)) {
        return entry->ranges;
    }

    auto* entry = new UrlRangesCacheEntry;
    entry->ranges = urlRanges(text);
    const QVector<TextRange> ranges = entry->ranges;
    m_urlRangesCache.insert(text, entry, textCacheCost(text));
    return ranges;
}

void ChatItemDelegate::configureTextDocument(QTextDocument& document,
                                             const QString& text,
                                             const QFont& font,
                                             int textWidth,
                                             const QColor& textColor,
                                             bool isFromMe,
                                             bool dark) const
{
    document.setUndoRedoEnabled(false);
    document.setDocumentMargin(0);
    document.setDefaultFont(font);

    QTextOption textOption;
    textOption.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    document.setDefaultTextOption(textOption);
    document.setPlainText(documentTextForLayout(text));
    document.setTextWidth(qMax(1, textWidth));

    QTextCursor cursor(&document);
    cursor.select(QTextCursor::Document);
    QTextCharFormat textFormat;
    textFormat.setForeground(textColor);
    cursor.mergeCharFormat(textFormat);

    const QColor urlColor = linkTextColor(dark, isFromMe);
    const QVector<TextRange> urls = cachedUrlRanges(text);
    for (const TextRange& url : urls) {
        QTextCharFormat linkFormat;
        linkFormat.setForeground(urlColor);
        linkFormat.setUnderlineStyle(QTextCharFormat::SingleUnderline);

        cursor.clearSelection();
        cursor.setPosition(url.start);
        cursor.setPosition(url.start + url.length, QTextCursor::KeepAnchor);
        cursor.mergeCharFormat(linkFormat);
    }
}

QVector<ChatItemDelegate::TextRange> ChatItemDelegate::urlRanges(const QString& text) const
{
    static const QRegularExpression urlRegex(
            QStringLiteral(R"(\b((?:https?://|www\.)[A-Za-z0-9\-._~:/?#\[\]@!$&'()*+,;=%]+))"),
            QRegularExpression::CaseInsensitiveOption);

    QVector<TextRange> ranges;
    QRegularExpressionMatchIterator it = urlRegex.globalMatch(text);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        const QString rawUrl = match.captured(1);
        const QString url = SelectableText::trimmedUrlText(rawUrl);
        if (url.isEmpty()) {
            continue;
        }

        TextRange range;
        range.start = match.capturedStart(1);
        range.length = url.size();
        range.text = url;
        ranges.push_back(range);
    }
    return ranges;
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
