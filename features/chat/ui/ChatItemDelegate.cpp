#include "ChatItemDelegate.h"
#include "shared/services/AppFonts.h"
#include "app/state/CurrentUser.h"
#include "features/chat/model/ChatListModel.h"
#include "features/friend/data/UserRepository.h"
#include "ChatArea.h"
#include "shared/services/ImageService.h"
#include "shared/theme/ThemeManager.h"
#include "shared/ui/GlobalNotification.h"
#include "shared/ui/renderers/MediaPlaceholderRenderer.h"
#include <QAbstractTextDocumentLayout>
#include <QPainter>
#include <QRegularExpression>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextLayout>
#include <QTextOption>
#include <QFontMetrics>
#include <QApplication>
#include <QMouseEvent>
#include <QClipboard>
#include <QDateTime>
#include <QPainterPath>
#include <QPersistentModelIndex>
#include <QMenu>
#include <QKeyEvent>
#include <QtMath>
#include <utility>

namespace {

QColor groupRoleBackgroundColor(GroupRole role)
{
    if (role == GroupRole::Owner) {
        return ThemeManager::instance().color(ThemeColor::RoleOwnerBackground);
    }
    if (role == GroupRole::Admin && !ThemeManager::instance().isDark()) {
        return ThemeManager::instance().color(ThemeColor::RoleAdminBackground);
    }
    return ThemeManager::instance().color(ThemeColor::PanelRaisedBackground);
}

void showCopyNotification(QObject* owner)
{
    QWidget* widget = qobject_cast<QWidget*>(owner);
    GlobalNotification::showSuccess(widget, QStringLiteral("复制成功"));
}

bool copyMessageToClipboard(const ChatMessage* message)
{
    if (!message) {
        return false;
    }

    if (message->getType() == MessageType::Text) {
        const auto* textMessage = static_cast<const TextMessage*>(message);
        QApplication::clipboard()->setText(textMessage->getText());
        return true;
    }

    if (message->getType() == MessageType::Image) {
        const auto* imageMessage = static_cast<const ImageMessage*>(message);
        const QPixmap pixmap = ImageService::instance().pixmap(imageMessage->getImageSource());
        if (pixmap.isNull()) {
            return false;
        }
        QApplication::clipboard()->setPixmap(pixmap);
        return true;
    }

    return false;
}

QColor linkTextColor(bool dark, bool isFromMe, const QColor& textColor)
{
    if (isFromMe) {
        return textColor;
    }

    Q_UNUSED(dark);
    return ThemeManager::instance().color(ThemeColor::AccentLinkText);
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

QString roleText(GroupRole role)
{
    switch (role) {
    case GroupRole::Owner:
        return QStringLiteral("群主");
    case GroupRole::Admin:
        return QStringLiteral("管理员");
    case GroupRole::Member:
    default:
        return QStringLiteral("群成员");
    }
}

bool isGroupSystemEventMessage(const ChatMessage* message)
{
    if (!message) {
        return false;
    }
    return message->getType() == MessageType::GroupMemberJoined ||
           message->getType() == MessageType::GroupSystemEvent;
}

QSize imageBubbleSize(const ImageMessage* message, int maxWidth)
{
    constexpr int kImageMaxHeight = 200;
    QSize sourceSize = ImageService::instance().sourceSize(message->getImageSource());
    if (!sourceSize.isValid()) {
        sourceSize = QSize(qMin(maxWidth, 180), 135);
    }

    return sourceSize.scaled(maxWidth,
                             kImageMaxHeight,
                             Qt::KeepAspectRatio);
}

void drawPlainImagePlaceholder(QPainter* painter, const QRect& rect, int radius)
{
    if (!painter || rect.isEmpty()) {
        return;
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::ImagePlaceholder));
    painter->drawRoundedRect(rect, radius, radius);
    painter->restore();
}

QString referenceSenderName(const ChatMessage* referencedMessage)
{
    if (!referencedMessage) {
        return {};
    }

    QString senderName = referencedMessage->getSenderName().trimmed();
    if (senderName.isEmpty()) {
        senderName = referencedMessage->getSenderId();
    }
    return senderName;
}

QString referenceSenderLabel(const ChatMessage* referencedMessage)
{
    return QStringLiteral("%1：").arg(referenceSenderName(referencedMessage));
}

QString referenceDisplayText(const ChatMessage* referencedMessage)
{
    if (!referencedMessage || referencedMessage->getType() == MessageType::Recall) {
        return QStringLiteral("该消息已撤回");
    }

    const QString content = referencedMessage->getType() == MessageType::Image
            ? QStringLiteral("[图片]")
            : referencedMessage->getContent().simplified();
    return QStringLiteral("%1：%2").arg(referenceSenderName(referencedMessage), content);
}

void drawElidedTextLines(QPainter* painter,
                         const QRect& rect,
                         const QString& text,
                         const QFont& font,
                         int maxLines)
{
    if (!painter || rect.isEmpty() || text.isEmpty() || maxLines <= 0) {
        return;
    }

    const QFontMetrics fm(font);
    QTextLayout layout(text, font);
    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    layout.setTextOption(option);

    QVector<QTextLine> lines;
    layout.beginLayout();
    for (int i = 0; i < maxLines; ++i) {
        QTextLine line = layout.createLine();
        if (!line.isValid()) {
            break;
        }
        line.setLineWidth(rect.width());
        lines.push_back(line);
    }
    layout.endLayout();

    const int lineHeight = fm.lineSpacing();
    int y = rect.top() + qMax(0, (rect.height() - lineHeight * lines.size()) / 2);
    for (int i = 0; i < lines.size(); ++i) {
        const QTextLine& line = lines.at(i);
        const bool isLastPaintedLine = i == lines.size() - 1;
        const bool hasHiddenText = line.textStart() + line.textLength() < text.size();
        if (isLastPaintedLine && hasHiddenText) {
            painter->drawText(QRect(rect.left(), y, rect.width(), lineHeight),
                              Qt::AlignLeft | Qt::AlignVCenter,
                              fm.elidedText(text.mid(line.textStart()), Qt::ElideRight, rect.width()));
        } else {
            line.draw(painter, QPointF(rect.left(), y));
        }
        y += lineHeight;
    }
}

struct GroupSystemEventSegment {
    QString text;
    QString userId;
    bool highlighted = false;
};

QVector<GroupSystemEventSegment> groupSystemEventSegments(const ChatMessage* message)
{
    if (!message) {
        return {};
    }
    if (message->getType() == MessageType::GroupMemberJoined) {
        const auto* joinedMessage = static_cast<const GroupMemberJoinedMessage*>(message);
        QVector<GroupSystemEventSegment> segments;
        if (!joinedMessage->getInviterName().isEmpty()) {
            segments.push_back(GroupSystemEventSegment{
                    joinedMessage->getInviterName(),
                    joinedMessage->getInviterId(),
                    true
            });
            segments.push_back(GroupSystemEventSegment{QStringLiteral("邀请了"), QString(), false});
        }
        segments.push_back(GroupSystemEventSegment{
                joinedMessage->getMemberName(),
                joinedMessage->getMemberId(),
                true
        });
        segments.push_back(GroupSystemEventSegment{QStringLiteral("加入群聊"), QString(), false});
        return segments;
    }
    if (message->getType() == MessageType::GroupSystemEvent) {
        const auto* eventMessage = static_cast<const GroupSystemEventMessage*>(message);
        QVector<GroupSystemEventSegment> segments;
        if (!eventMessage->getPrefix().isEmpty()) {
            segments.push_back(GroupSystemEventSegment{eventMessage->getPrefix(), QString(), false});
        }
        if (!eventMessage->getHighlightedName().isEmpty()) {
            segments.push_back(GroupSystemEventSegment{
                    eventMessage->getHighlightedName(),
                    eventMessage->getHighlightedUserId(),
                    true
            });
        }
        if (!eventMessage->getSuffix().isEmpty()) {
            segments.push_back(GroupSystemEventSegment{eventMessage->getSuffix(), QString(), false});
        }
        return segments;
    }
    return {};
}

} // namespace

ChatItemDelegate::ChatItemDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
    m_textDocumentCache.setMaxCost(96);
    m_textSizeCache.setMaxCost(256);
    m_urlRangesCache.setMaxCost(256);
}

void ChatItemDelegate::setRecallEligibilityCallback(std::function<bool(const ChatMessage*)> callback)
{
    m_recallEligibilityCallback = std::move(callback);
}

void ChatItemDelegate::setReferenceResolver(std::function<const ChatMessage*(const QString&)> resolver)
{
    m_referenceResolver = std::move(resolver);
}

void ChatItemDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                           const QModelIndex& index) const
{
    QVariant data = index.data(Qt::UserRole);
    const ChatMessage* message = nullptr;
    const TimeHeader* timeHeader = nullptr;
    const NewMessageDivider* newMessageDivider = nullptr;
    const LoadingPlaceholder* loadingPlaceholder = nullptr;

    if (data.canConvert<TimeHeader*>()) {
        timeHeader = data.value<TimeHeader*>();
    } else if (data.canConvert<NewMessageDivider*>()) {
        newMessageDivider = data.value<NewMessageDivider*>();
    } else if (data.canConvert<LoadingPlaceholder*>()) {
        loadingPlaceholder = data.value<LoadingPlaceholder*>();
    } else if (data.canConvert<ChatMessage*>()) {
        message = data.value<ChatMessage*>();
    }

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);
    AppFonts::configurePainterForText(*painter);

    if (loadingPlaceholder) {
        drawLoadingPlaceholder(painter, option.rect, loadingPlaceholder);
    } else if (newMessageDivider) {
        drawNewMessageDivider(painter, option.rect, newMessageDivider->text);
    } else if (timeHeader) {
        // 绘制时间标识
        QRect timeHeaderRect = QRect(
            0,                          // x
            option.rect.y() + 6,        // y（顶部留出6像素间距）
            option.rect.width(),        // width
            TIME_HEADER_HEIGHT          // height
        );
        drawTimeHeader(painter, timeHeaderRect, timeHeader->text);
    } else if (message) {
        if (index.data(Qt::UserRole + 2).toBool()) {
            drawRowHighlight(painter, option.rect);
        }

        if (isGroupSystemEventMessage(message)) {
            drawGroupSystemEventMessage(painter, option.rect, message);
            painter->restore();
            return;
        }

        if (message->getType() == MessageType::Recall) {
            drawRecallMessage(painter, option.rect, static_cast<const RecallMessage*>(message), index);
            painter->restore();
            return;
        }

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

        const ChatMessage* referencedMessage = m_referenceResolver && !message->getReferencedMessageId().isEmpty()
                ? m_referenceResolver(message->getReferencedMessageId())
                : nullptr;
        const QRect referenceRect = calculateReferenceRect(option.rect, message, referencedMessage);
        if (!referenceRect.isEmpty()) {
            drawMessageReference(painter, referenceRect, referencedMessage);
        }
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
            if (chatModel->isBottomSpace(index.row()) ||
                    chatModel->isTimeHeader(index.row()) ||
                    chatModel->isLoadingPlaceholder(index.row()) ||
                    chatModel->isNewMessageDivider(index.row())) {
                chatModel->clearSelection();
                return true;
            }
        }

        const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
        if (!message) return false;

        if (message->getType() == MessageType::Recall) {
            const RecallMessage* recallMessage = static_cast<const RecallMessage*>(message);
            if (mouseEvent->button() == Qt::LeftButton &&
                    recallMessage->canReedit() &&
                    calculateRecallReeditRect(option.rect, recallMessage).contains(mouseEvent->pos())) {
                emit reeditRequested(index.row());
                return true;
            }
            model->setData(index, false, Qt::UserRole + 1);
            return true;
        }
        if (isGroupSystemEventMessage(message)) {
            model->setData(index, false, Qt::UserRole + 1);
            return true;
        }

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
            if (message && message->getIsSelected() && copyMessageToClipboard(message)) {
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
    if (message->getType() == MessageType::Recall) {
        const RecallMessage* recallMessage = static_cast<const RecallMessage*>(message);
        return calculateRecallContentRect(option.rect, recallMessage).contains(viewportPos);
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
            ? ThemeManager::textColorOn(ThemeManager::instance().color(ThemeColor::AccentBubble))
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

QString ChatItemDelegate::imageSourceAt(const QStyleOptionViewItem& option,
                                        const QModelIndex& index,
                                        const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Image) {
        return {};
    }
    if (!bubbleHitTest(option, index, viewportPos)) {
        return {};
    }

    return static_cast<const ImageMessage*>(message)->getImageSource();
}

QString ChatItemDelegate::referencedImageSourceAt(const QStyleOptionViewItem& option,
                                                  const QModelIndex& index,
                                                  const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getReferencedMessageId().isEmpty()) {
        return {};
    }

    const ChatMessage* referencedMessage = m_referenceResolver
            ? m_referenceResolver(message->getReferencedMessageId())
            : nullptr;
    if (!referencedMessage || referencedMessage->getType() != MessageType::Image) {
        return {};
    }

    const QRect referenceRect = calculateReferenceRect(option.rect, message, referencedMessage);
    if (referenceRect.isEmpty()) {
        return {};
    }

    const QRect imageRect = calculateReferenceImageRect(referenceRect, referencedMessage);
    if (!imageRect.contains(viewportPos)) {
        return {};
    }

    return static_cast<const ImageMessage*>(referencedMessage)->getImageSource();
}

QString ChatItemDelegate::referencedMessageIdAt(const QStyleOptionViewItem& option,
                                                const QModelIndex& index,
                                                const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getReferencedMessageId().isEmpty()) {
        return {};
    }

    const ChatMessage* referencedMessage = m_referenceResolver
            ? m_referenceResolver(message->getReferencedMessageId())
            : nullptr;
    const QRect referenceRect = calculateReferenceRect(option.rect, message, referencedMessage);
    return referenceRect.contains(viewportPos) ? message->getReferencedMessageId() : QString();
}

QString ChatItemDelegate::groupSystemEventUserIdAt(const QStyleOptionViewItem& option,
                                                   const QModelIndex& index,
                                                   const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!isGroupSystemEventMessage(message)) {
        return {};
    }

    const QVector<GroupSystemEventLayoutSegment> segments =
            groupSystemEventLayoutSegments(option.rect, message);
    for (const GroupSystemEventLayoutSegment& segment : segments) {
        if (segment.highlighted &&
                !segment.userId.isEmpty() &&
                segment.rect.adjusted(-3, -4, 3, 4).contains(viewportPos)) {
            return segment.userId;
        }
    }
    return {};
}

bool ChatItemDelegate::avatarHitTest(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    return avatarRectForIndex(option, index).contains(viewportPos);
}

QRect ChatItemDelegate::avatarRectForIndex(const QStyleOptionViewItem& option,
                                           const QModelIndex& index) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message ||
            message->getType() == MessageType::Recall ||
            isGroupSystemEventMessage(message)) {
        return {};
    }

    return calculateAvatarRect(option.rect, message->isFromMe());
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

bool ChatItemDelegate::reeditHitTest(const QStyleOptionViewItem& option,
                                     const QModelIndex& index,
                                     const QPoint& viewportPos) const
{
    const ChatMessage* message = index.data(Qt::UserRole).value<ChatMessage*>();
    if (!message || message->getType() != MessageType::Recall) {
        return false;
    }

    const RecallMessage* recallMessage = static_cast<const RecallMessage*>(message);
    if (!recallMessage->canReedit()) {
        return false;
    }

    return calculateRecallReeditRect(option.rect, recallMessage).adjusted(-4, -4, 4, 4).contains(viewportPos);
}

bool ChatItemDelegate::triggerReeditIfHit(const QStyleOptionViewItem& option,
                                          const QModelIndex& index,
                                          const QPoint& viewportPos)
{
    if (!reeditHitTest(option, index, viewportPos)) {
        return false;
    }

    emit reeditRequested(index.row());
    return true;
}

void ChatItemDelegate::drawBubble(QPainter* painter, const QRect& rect,
                                  bool isFromMe, const ChatMessage* message, bool isSelected,
                                  const QModelIndex& index) const
{
    if (message->getType() == MessageType::Recall) {
        return;
    }
    if (message->getType() == MessageType::Image) {
        const ImageMessage* imgMsg = static_cast<const ImageMessage*>(message);
        drawImageMessage(painter, rect, imgMsg->getImageSource(), isSelected);
        return;
    }

    QColor bubbleColor;
    const bool dark = ThemeManager::instance().isDark();
    if (isFromMe) {
        const QColor baseBubbleColor = ThemeManager::instance().color(ThemeColor::AccentBubble);
        bubbleColor = isSelected ? baseBubbleColor.darker(118)
                                 : baseBubbleColor;
    } else if (dark) {
        bubbleColor = isSelected
                ? ThemeManager::instance().color(ThemeColor::MessageBubblePeerSelected)
                : ThemeManager::instance().color(ThemeColor::MessageBubblePeer);
    } else {
        bubbleColor = isSelected
                ? ThemeManager::instance().color(ThemeColor::MessageBubblePeerSelected)
                : ThemeManager::instance().color(ThemeColor::MessageBubblePeer);
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
        const QColor textColor = isFromMe
                ? ThemeManager::textColorOn(bubbleColor)
                : ThemeManager::instance().color(ThemeColor::PrimaryText);
        drawTextMessage(painter, rect, message->getContent(), isFromMe, textColor, index);
    }
}

void ChatItemDelegate::drawMessageReference(QPainter* painter,
                                            const QRect& rect,
                                            const ChatMessage* referencedMessage) const
{
    if (rect.isEmpty()) {
        return;
    }

    painter->save();
    painter->setPen(Qt::NoPen);
    painter->setBrush(ThemeManager::instance().color(ThemeColor::MessageReferenceBackground));
    painter->drawRoundedRect(rect, REFERENCE_RADIUS, REFERENCE_RADIUS);

    const QFont font = referenceFont();
    painter->setFont(font);
    painter->setPen(ThemeManager::instance().color(ThemeColor::MessageReferenceText));

    if (referencedMessage && referencedMessage->getType() == MessageType::Image) {
        const auto* imageMessage = static_cast<const ImageMessage*>(referencedMessage);
        const QRect imageRect = calculateReferenceImageRect(rect, referencedMessage);
        if (!imageRect.isEmpty()) {
            const QFontMetrics fm(font);
            const QString senderLabel = referenceSenderLabel(referencedMessage);
            const QRect senderRect(rect.left() + REFERENCE_PADDING,
                                   rect.top() + REFERENCE_PADDING,
                                   rect.width() - REFERENCE_PADDING * 2,
                                   fm.height());
            painter->drawText(senderRect,
                              Qt::AlignLeft | Qt::AlignVCenter,
                              fm.elidedText(senderLabel,
                                            Qt::ElideRight,
                                            senderRect.width()));

            const QPixmap image = ImageService::instance().centerCrop(imageMessage->getImageSource(),
                                                                      imageRect.size(),
                                                                      REFERENCE_IMAGE_RADIUS,
                                                                      painter->device()->devicePixelRatioF());
            QPainterPath clipPath;
            clipPath.addRoundedRect(imageRect, REFERENCE_IMAGE_RADIUS, REFERENCE_IMAGE_RADIUS);
            painter->setClipPath(clipPath);
            if (image.isNull()) {
                drawPlainImagePlaceholder(painter, imageRect, REFERENCE_IMAGE_RADIUS);
            } else {
                painter->drawPixmap(imageRect, image);
            }
            painter->restore();
            return;
        }
    }

    const QRect textRect = rect.adjusted(REFERENCE_PADDING,
                                         REFERENCE_PADDING,
                                         -REFERENCE_PADDING,
                                         -REFERENCE_PADDING);
    drawElidedTextLines(painter, textRect, referenceDisplayText(referencedMessage), font, 2);
    painter->restore();
}

void ChatItemDelegate::drawRowHighlight(QPainter* painter, const QRect& rect) const
{
    if (!painter || rect.isEmpty()) {
        return;
    }

    painter->save();
    QColor color = ThemeManager::instance().color(ThemeColor::Accent);
    color.setAlpha(ThemeManager::instance().isDark() ? 58 : 44);
    painter->setPen(Qt::NoPen);
    painter->setBrush(color);
    painter->drawRect(rect.adjusted(0, 1, 0, -1));
    painter->restore();
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
    if (avatar.isNull()) {
        MediaPlaceholderRenderer::drawAvatar(painter, rect);
    } else {
        painter->drawPixmap(rect, avatar);
    }
}

void ChatItemDelegate::drawTextMessage(QPainter* painter, const QRect& rect,
                                       const QString& text, bool isFromMe,
                                       const QColor& textColor,
                                       const QModelIndex& index) const
{
    const QRect textRect = calculateTextRect(rect);
    const bool dark = ThemeManager::instance().isDark();
    const QColor selectionColor = isFromMe
            ? ThemeManager::instance().color(ThemeColor::AccentTextSelectionOnAccent)
            : ThemeManager::instance().color(ThemeColor::AccentTextSelection);

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

void ChatItemDelegate::drawRecallMessage(QPainter* painter,
                                         const QRect& rect,
                                         const RecallMessage* message,
                                         const QModelIndex&) const
{
    if (!message) {
        return;
    }

    painter->save();
    AppFonts::configurePainterForText(*painter);
    const QFont font = recallFont();
    painter->setFont(font);
    const QFontMetrics fm(font);
    const QRect contentRect = calculateRecallContentRect(rect, message);
    const QString displayText = message->getDisplayText();
    const QColor normalColor = ThemeManager::instance().color(ThemeColor::TertiaryText);
    const QColor accentColor = ThemeManager::instance().color(ThemeColor::Accent);

    int x = contentRect.left();
    const int baselineY = contentRect.top() + (contentRect.height() + fm.ascent() - fm.descent()) / 2;

    if (message->isInGroupChat()) {
        const QString name = message->isModeratorRecall()
                ? message->getActorName()
                : message->getSenderName();
        const QString prefix = message->isModeratorRecall()
                ? QStringLiteral("%1 ").arg(roleText(message->getActorRole()))
                : QStringLiteral("%1 ").arg(roleText(message->getRole()));
        const QString suffix = message->isModeratorRecall()
                ? QStringLiteral(" 撤回了一条群成员消息")
                : QStringLiteral(" 撤回了一条消息");

        painter->setPen(normalColor);
        painter->drawText(x, baselineY, prefix);
        x += fm.horizontalAdvance(prefix);
        painter->setPen(accentColor);
        painter->drawText(x, baselineY, name);
        x += fm.horizontalAdvance(name);
        painter->setPen(normalColor);
        painter->drawText(x, baselineY, suffix);
    } else {
        painter->setPen(normalColor);
        painter->drawText(x, baselineY, displayText);
    }

    if (message->canReedit()) {
        const QRect reeditRect = calculateRecallReeditRect(rect, message);
        painter->setPen(accentColor);
        painter->drawText(reeditRect, Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("重新编辑"));
    }

    painter->restore();
}

void ChatItemDelegate::drawGroupSystemEventMessage(QPainter* painter,
                                                   const QRect& rect,
                                                   const ChatMessage* message) const
{
    const QVector<GroupSystemEventLayoutSegment> segments =
            groupSystemEventLayoutSegments(rect, message);
    if (segments.isEmpty()) {
        return;
    }

    painter->save();
    AppFonts::configurePainterForText(*painter);
    const QFont font = recallFont();
    painter->setFont(font);
    const QFontMetrics fm(font);

    const QColor normalColor = ThemeManager::instance().color(ThemeColor::TertiaryText);
    const QColor accentColor = ThemeManager::instance().color(ThemeColor::Accent);
    const int baselineY = segments.first().rect.top() +
            (segments.first().rect.height() + fm.ascent() - fm.descent()) / 2;

    for (const GroupSystemEventLayoutSegment& segment : segments) {
        if (segment.text.isEmpty()) {
            continue;
        }
        painter->setPen(segment.highlighted ? accentColor : normalColor);
        painter->drawText(segment.rect.left(), baselineY, segment.text);
    }
    painter->restore();
}

void ChatItemDelegate::drawImageMessage(QPainter* painter, const QRect& rect,
                                      const QString& imageSource,
                                      bool isSelected) const
{
    const QPixmap image = ImageService::instance().scaled(imageSource,
                                                          rect.size(),
                                                          Qt::KeepAspectRatio,
                                                          painter->device()->devicePixelRatioF());

    QPainterPath clipPath;
    clipPath.addRoundedRect(rect, IMAGE_RADIUS, IMAGE_RADIUS);
    painter->save();
    painter->setClipPath(clipPath);
    if (image.isNull()) {
        drawPlainImagePlaceholder(painter, rect, IMAGE_RADIUS);
    } else {
        painter->drawPixmap(rect, image);
    }
    if (isSelected) {
        painter->fillRect(rect, ThemeManager::instance().color(ThemeColor::ScrollThumb));
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
        QColor textColor = (role == GroupRole::Owner)
                ? ThemeManager::instance().color(ThemeColor::RoleOwnerText)
                : ThemeManager::instance().color(ThemeColor::RoleAdminText);

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
        QColor textColor = (role == GroupRole::Owner)
                ? ThemeManager::instance().color(ThemeColor::RoleOwnerText)
                : ThemeManager::instance().color(ThemeColor::RoleAdminText);

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
    const NewMessageDivider* newMessageDivider = nullptr;
    const LoadingPlaceholder* loadingPlaceholder = nullptr;

    if (data.canConvert<TimeHeader*>()) {
        timeHeader = data.value<TimeHeader*>();
    } else if (data.canConvert<NewMessageDivider*>()) {
        newMessageDivider = data.value<NewMessageDivider*>();
    } else if (data.canConvert<LoadingPlaceholder*>()) {
        loadingPlaceholder = data.value<LoadingPlaceholder*>();
    } else if (data.canConvert<ChatMessage*>()) {
        message = data.value<ChatMessage*>();
    } else if (data.canConvert<int>()) {
        // 处理底部空白项
        int height = data.toInt();
        return QSize(option.rect.width(), height);
    }

    if (loadingPlaceholder) {
        return QSize(option.rect.width(), LOADING_PLACEHOLDER_HEIGHT);
    } else if (newMessageDivider) {
        return QSize(option.rect.width(), NEW_MESSAGE_DIVIDER_HEIGHT);
    } else if (timeHeader) {
        // 时间标识的高度（包括上下间距）
        return QSize(option.rect.width(), TIME_HEADER_HEIGHT + 12);  // 12是上下各6像素的间距
    } else if (message) {
        if (isGroupSystemEventMessage(message)) {
            return QSize(option.rect.width(), GROUP_EVENT_HEIGHT);
        }
        if (message->getType() == MessageType::Recall) {
            return QSize(option.rect.width(), 32);
        }

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
            bubbleHeight = imageBubbleSize(imgMsg, maxBubbleWidth).height();
        }

        int referenceHeight = 0;
        if (!message->getReferencedMessageId().isEmpty()) {
            const ChatMessage* referencedMessage = m_referenceResolver
                    ? m_referenceResolver(message->getReferencedMessageId())
                    : nullptr;
            const QRect referenceRect = calculateReferenceRect(option.rect,
                                                               message,
                                                               referencedMessage);
            if (!referenceRect.isEmpty()) {
                referenceHeight = REFERENCE_GAP + referenceRect.height();
            }
        }

        const int contentHeight = qMax(AVATAR_SIZE, groupInfoHeight + bubbleHeight + referenceHeight);
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
        const QSize scaledSize = imageBubbleSize(imgMsg, maxWidth);
        bubbleWidth = scaledSize.width();
        bubbleHeight = scaledSize.height();
    }

    bubbleWidth = qMin(bubbleWidth, maxWidth);
    const int x = isFromMe
            ? contentRect.right() - HORIZONTAL_EDGE_MARGIN - AVATAR_SIZE - BUBBLE_MARGIN - bubbleWidth + 1
            : HORIZONTAL_EDGE_MARGIN + AVATAR_SIZE + BUBBLE_MARGIN;

    return QRect(x, contentRect.y() + BUBBLE_MARGIN,
                bubbleWidth, bubbleHeight);
}

QRect ChatItemDelegate::calculatePaintedBubbleRect(const QRect& contentRect,
                                                   const ChatMessage* message,
                                                   int maxWidth) const
{
    if (!message ||
            message->getType() == MessageType::Recall ||
            isGroupSystemEventMessage(message)) {
        return {};
    }

    QRect bubbleRect = calculateBubbleRect(contentRect, message, maxWidth, message->isFromMe());
    if (message->isInGroupChat()) {
        bubbleRect.moveTop(bubbleRect.top() + NAME_HEIGHT + GROUP_INFO_GAP);
    }
    return bubbleRect;
}

QRect ChatItemDelegate::calculateReferenceRect(const QRect& contentRect,
                                               const ChatMessage* message,
                                               const ChatMessage* referencedMessage) const
{
    if (!message || message->getReferencedMessageId().isEmpty()) {
        return {};
    }
    if (message->getType() == MessageType::Recall || isGroupSystemEventMessage(message)) {
        return {};
    }

    const int maxBubbleWidth = calculateMaxBubbleWidth(contentRect);
    const QRect bubbleRect = calculatePaintedBubbleRect(contentRect, message, maxBubbleWidth);
    if (bubbleRect.isEmpty()) {
        return {};
    }

    const int maxContentWidth = qMax(1, maxBubbleWidth - REFERENCE_PADDING * 2);
    QSize contentSize;
    if (referencedMessage && referencedMessage->getType() == MessageType::Image) {
        const auto* imageMessage = static_cast<const ImageMessage*>(referencedMessage);
        const QSize sourceSize = ImageService::instance().sourceSize(imageMessage->getImageSource());
        if (sourceSize.isValid()) {
            const QFontMetrics fm(referenceFont());
            const int imageSide = qMin(maxContentWidth,
                                       fm.lineSpacing() * REFERENCE_IMAGE_MAX_LINES);
            contentSize = QSize(qMax(fm.horizontalAdvance(referenceSenderLabel(referencedMessage)),
                                      imageSide),
                                fm.height() + REFERENCE_IMAGE_GAP + imageSide);
        }
    }
    if (!contentSize.isValid()) {
        const QSize textSize = textDocumentSize(referenceDisplayText(referencedMessage),
                                                referenceFont(),
                                                maxContentWidth);
        const QFontMetrics fm(referenceFont());
        contentSize = QSize(textSize.width(),
                            qMin(textSize.height(), fm.lineSpacing() * 2));
    }

    const int width = qBound(1,
                             contentSize.width() + REFERENCE_PADDING * 2,
                             maxBubbleWidth);
    const int height = contentSize.height() + REFERENCE_PADDING * 2;
    const int x = message->isFromMe()
            ? bubbleRect.right() - width + 1
            : bubbleRect.left();
    return QRect(x, bubbleRect.bottom() + REFERENCE_GAP + 1, width, height);
}

QRect ChatItemDelegate::calculateReferenceImageRect(const QRect& referenceRect,
                                                    const ChatMessage* referencedMessage) const
{
    if (referenceRect.isEmpty() ||
            !referencedMessage ||
            referencedMessage->getType() != MessageType::Image) {
        return {};
    }

    const auto* imageMessage = static_cast<const ImageMessage*>(referencedMessage);
    const QSize sourceSize = ImageService::instance().sourceSize(imageMessage->getImageSource());
    if (!sourceSize.isValid()) {
        return {};
    }

    const QFontMetrics fm(referenceFont());
    const int contentWidth = qMax(1, referenceRect.width() - REFERENCE_PADDING * 2);
    const int imageSide = qMin(contentWidth, fm.lineSpacing() * REFERENCE_IMAGE_MAX_LINES);
    if (imageSide <= 0) {
        return {};
    }

    return QRect(referenceRect.left() + REFERENCE_PADDING,
                 referenceRect.top() + REFERENCE_PADDING + fm.height() + REFERENCE_IMAGE_GAP,
                 imageSide,
                 imageSide);
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

QRect ChatItemDelegate::calculateRecallContentRect(const QRect& contentRect,
                                                   const RecallMessage* message) const
{
    if (!message) {
        return {};
    }

    const QFontMetrics fm(recallFont());
    int textWidth = 0;
    if (message->isInGroupChat()) {
        const QString name = message->isModeratorRecall()
                ? message->getActorName()
                : message->getSenderName();
        const QString prefix = message->isModeratorRecall()
                ? QStringLiteral("%1 ").arg(roleText(message->getActorRole()))
                : QStringLiteral("%1 ").arg(roleText(message->getRole()));
        const QString suffix = message->isModeratorRecall()
                ? QStringLiteral(" 撤回了一条群成员消息")
                : QStringLiteral(" 撤回了一条消息");
        textWidth = fm.horizontalAdvance(prefix) +
                fm.horizontalAdvance(name) +
                fm.horizontalAdvance(suffix);
    } else {
        textWidth = fm.horizontalAdvance(message->getDisplayText());
    }

    if (message->canReedit()) {
        textWidth += fm.horizontalAdvance(QStringLiteral(" 重新编辑"));
    }

    const int width = qMin(textWidth, qMax(0, contentRect.width() - HORIZONTAL_EDGE_MARGIN * 2));
    return QRect(contentRect.left() + (contentRect.width() - width) / 2,
                 contentRect.top(),
                 width,
                 contentRect.height());
}

QRect ChatItemDelegate::calculateRecallReeditRect(const QRect& contentRect,
                                                  const RecallMessage* message) const
{
    if (!message || !message->canReedit()) {
        return {};
    }

    const QFontMetrics fm(recallFont());
    int leftWidth = 0;
    if (message->isInGroupChat()) {
        const QString name = message->isModeratorRecall()
                ? message->getActorName()
                : message->getSenderName();
        const QString prefix = message->isModeratorRecall()
                ? QStringLiteral("%1 ").arg(roleText(message->getActorRole()))
                : QStringLiteral("%1 ").arg(roleText(message->getRole()));
        const QString suffix = message->isModeratorRecall()
                ? QStringLiteral(" 撤回了一条群成员消息 ")
                : QStringLiteral(" 撤回了一条消息 ");
        leftWidth = fm.horizontalAdvance(prefix) +
                fm.horizontalAdvance(name) +
                fm.horizontalAdvance(suffix);
    } else {
        leftWidth = fm.horizontalAdvance(message->getDisplayText() + QStringLiteral(" "));
    }

    const QRect fullRect = calculateRecallContentRect(contentRect, message);
    return QRect(fullRect.left() + leftWidth,
                 fullRect.top(),
                 fm.horizontalAdvance(QStringLiteral("重新编辑")),
                 fullRect.height());
}

QRect ChatItemDelegate::calculateGroupSystemEventContentRect(
        const QRect& contentRect,
        const ChatMessage* message) const
{
    const QVector<GroupSystemEventSegment> segments = groupSystemEventSegments(message);
    if (segments.isEmpty()) {
        return {};
    }

    const QFontMetrics fm(recallFont());
    const int maxWidth = qMax(0, contentRect.width() - HORIZONTAL_EDGE_MARGIN * 2);
    int fullTextWidth = 0;
    for (const GroupSystemEventSegment& segment : segments) {
        fullTextWidth += fm.horizontalAdvance(segment.text);
    }
    const int width = qMin(fullTextWidth, maxWidth);
    return QRect(contentRect.left() + (contentRect.width() - width) / 2,
                 contentRect.top(),
                 width,
                 contentRect.height());
}

QVector<ChatItemDelegate::GroupSystemEventLayoutSegment>
ChatItemDelegate::groupSystemEventLayoutSegments(
        const QRect& contentRect,
        const ChatMessage* message) const
{
    const QVector<GroupSystemEventSegment> sourceSegments = groupSystemEventSegments(message);
    if (sourceSegments.isEmpty()) {
        return {};
    }

    const QFontMetrics fm(recallFont());
    const QRect layoutRect = calculateGroupSystemEventContentRect(contentRect, message);
    if (layoutRect.isEmpty()) {
        return {};
    }

    int fullWidth = 0;
    int fixedWidth = 0;
    int highlightedWidth = 0;
    for (const GroupSystemEventSegment& segment : sourceSegments) {
        const int width = fm.horizontalAdvance(segment.text);
        fullWidth += width;
        if (segment.highlighted) {
            highlightedWidth += width;
        } else {
            fixedWidth += width;
        }
    }

    const bool shouldElideHighlighted =
            fullWidth > layoutRect.width() && highlightedWidth > 0;
    const int availableHighlightedWidth =
            qMax(0, layoutRect.width() - fixedWidth);

    QVector<GroupSystemEventLayoutSegment> layoutSegments;
    layoutSegments.reserve(sourceSegments.size());
    int x = layoutRect.left();
    for (const GroupSystemEventSegment& segment : sourceSegments) {
        QString paintedText = segment.text;
        if (shouldElideHighlighted && segment.highlighted) {
            const int segmentWidth = fm.horizontalAdvance(segment.text);
            const int targetWidth = highlightedWidth <= 0
                    ? 0
                    : qMax(0, availableHighlightedWidth * segmentWidth / highlightedWidth);
            paintedText = fm.elidedText(segment.text, Qt::ElideRight, targetWidth);
        }

        const int width = fm.horizontalAdvance(paintedText);
        GroupSystemEventLayoutSegment layoutSegment;
        layoutSegment.text = paintedText;
        layoutSegment.userId = segment.userId;
        layoutSegment.highlighted = segment.highlighted;
        layoutSegment.rect = QRect(x, layoutRect.top(), width, layoutRect.height());
        layoutSegments.push_back(std::move(layoutSegment));
        x += width;
    }

    return layoutSegments;
}

QFont ChatItemDelegate::messageFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(14);
    return font;
}

QFont ChatItemDelegate::recallFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(12);
    return font;
}

QFont ChatItemDelegate::referenceFont() const
{
    QFont font = QApplication::font();
    font.setPixelSize(13);
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

    const QColor urlColor = linkTextColor(dark, isFromMe, textColor);
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
    bool hasPrimaryAction = false;
    if (message->getType() == MessageType::Text || message->getType() == MessageType::Image) {
        QAction* copyAction = menu->addAction(QStringLiteral("复制"));
        connect(copyAction, &QAction::triggered, [this, message, index, model = const_cast<QAbstractItemModel*>(index.model())]() {
            if (copyMessageToClipboard(message)) {
                showCopyNotification(parent());
            }
            model->setData(index, false, Qt::UserRole + 1);
        });
        hasPrimaryAction = true;
    }

    QAction* referenceAction = menu->addAction(QStringLiteral("引用"));
    connect(referenceAction, &QAction::triggered, [this, index]() {
        emit const_cast<ChatItemDelegate*>(this)->referenceRequested(index.row());
    });
    hasPrimaryAction = true;

    const bool canRecall = message &&
            message->getType() != MessageType::Recall &&
            m_recallEligibilityCallback &&
            m_recallEligibilityCallback(message);
    if (canRecall) {
        QAction* recallAction = menu->addAction(QStringLiteral("撤回"));
        connect(recallAction, &QAction::triggered, [this, index]() {
            emit const_cast<ChatItemDelegate*>(this)->recallRequested(index.row());
        });
        hasPrimaryAction = true;
    }

    if (hasPrimaryAction) {
        menu->addSeparator();
    }

    // 添加删除选项
    QAction* deleteAction = menu->addAction(QStringLiteral("删除"));
    StyledActionMenu::setActionColors(deleteAction,
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionText),
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionBackground),
                                      ThemeManager::instance().color(ThemeColor::DestructiveActionText));
    connect(deleteAction, &QAction::triggered, [this, index, model = index.model()]() {
        Q_UNUSED(model);
        emit const_cast<ChatItemDelegate*>(this)->deleteRequested(index.row());
    });

    // 菜单关闭后自动删除，并取消选中状态
    connect(menu, &StyledActionMenu::aboutToHide, menu, [menu, persistentIndex, model = const_cast<QAbstractItemModel*>(index.model())]() {
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

void ChatItemDelegate::drawNewMessageDivider(QPainter* painter, const QRect& rect,
                                             const QString& text) const
{
    QFont font = QApplication::font();
    font.setPixelSize(NEW_MESSAGE_DIVIDER_FONT_SIZE);
    painter->setFont(font);

    QFontMetrics metrics(font);
    const int textWidth = metrics.horizontalAdvance(text);
    const int textLeft = rect.left() + (rect.width() - textWidth) / 2;
    const int textRight = textLeft + textWidth;
    const int lineY = rect.top() + rect.height() / 2;
    const int leftStart = rect.left() + NEW_MESSAGE_DIVIDER_MARGIN;
    const int leftEnd = textLeft - NEW_MESSAGE_DIVIDER_TEXT_GAP;
    const int rightStart = textRight + NEW_MESSAGE_DIVIDER_TEXT_GAP;
    const int rightEnd = rect.right() - NEW_MESSAGE_DIVIDER_MARGIN;

    const QColor accent = ThemeManager::instance().color(ThemeColor::Accent);
    QPen linePen(accent, 1);
    linePen.setCapStyle(Qt::FlatCap);
    painter->setPen(linePen);
    if (leftEnd > leftStart) {
        painter->drawLine(QPoint(leftStart, lineY), QPoint(leftEnd, lineY));
    }
    if (rightEnd > rightStart) {
        painter->drawLine(QPoint(rightStart, lineY), QPoint(rightEnd, lineY));
    }

    painter->setPen(accent);
    painter->drawText(rect, Qt::AlignCenter, text);
}

void ChatItemDelegate::drawLoadingPlaceholder(QPainter* painter,
                                             const QRect& rect,
                                             const LoadingPlaceholder* placeholder) const
{
    const bool dark = ThemeManager::instance().isDark();
    const bool isFromMe = placeholder && placeholder->isFromMe;
    QColor base = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderBase);
    QColor highlight = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderHighlight);
    base.setAlpha(235);
    highlight.setAlpha(255);

    constexpr qint64 kShimmerPeriodMs = 1200;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const qint64 elapsed = placeholder && placeholder->shimmerStartedAtMs > 0
            ? qMax<qint64>(0, now - placeholder->shimmerStartedAtMs)
            : now;
    const qreal progress = static_cast<qreal>(elapsed % kShimmerPeriodMs) / kShimmerPeriodMs;
    const qreal center = -0.35 + progress * 1.7;
    auto shimmerBrush = [&](const QRectF& targetRect) {
        QLinearGradient gradient(targetRect.topLeft(), targetRect.topRight());
        gradient.setColorAt(0.0, base);
        gradient.setColorAt(qBound(0.0, center - 0.16, 1.0), base);
        gradient.setColorAt(qBound(0.0, center, 1.0), highlight);
        gradient.setColorAt(qBound(0.0, center + 0.16, 1.0), base);
        gradient.setColorAt(1.0, base);
        return QBrush(gradient);
    };

    const QRect avatarRect = calculateAvatarRect(rect, isFromMe);
    const QFontMetrics fm(messageFont());
    const int bubbleHeight = fm.height() + 2 * BUBBLE_PADDING;
    const int maxBubbleWidth = calculateMaxBubbleWidth(rect);
    const int preferredBubbleWidth = isFromMe ? rect.width() * 9 / 32 : rect.width() / 2;
    const int bubbleWidth = qMin(maxBubbleWidth, qMax(120, preferredBubbleWidth));
    const int bubbleX = isFromMe
            ? rect.right() - HORIZONTAL_EDGE_MARGIN - AVATAR_SIZE - BUBBLE_MARGIN - bubbleWidth + 1
            : avatarRect.right() + BUBBLE_MARGIN + 1;
    const QRect bubbleRect(bubbleX,
                           rect.top() + BUBBLE_MARGIN,
                           bubbleWidth,
                           bubbleHeight);

    painter->save();
    painter->setPen(Qt::NoPen);

    painter->setBrush(shimmerBrush(avatarRect));
    painter->drawEllipse(avatarRect);

    QPainterPath bubblePath;
    bubblePath.addRoundedRect(bubbleRect, BUBBLE_RADIUS, BUBBLE_RADIUS);
    painter->setBrush(shimmerBrush(bubbleRect));
    painter->drawPath(bubblePath);

    const int lineHeight = 9;
    const QRect line(bubbleRect.left() + BUBBLE_PADDING,
                     bubbleRect.top() + (bubbleRect.height() - lineHeight) / 2,
                     qMax(48, bubbleRect.width() * 3 / 5),
                     lineHeight);
    QColor lineBase = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderLineBase);
    lineBase.setAlpha(dark ? 210 : 220);
    QColor lineHighlight = ThemeManager::instance().color(ThemeColor::LoadingPlaceholderLineHighlight);
    lineHighlight.setAlpha(245);

    auto lineBrush = [&](const QRectF& targetRect) {
        QLinearGradient gradient(targetRect.topLeft(), targetRect.topRight());
        gradient.setColorAt(0.0, lineBase);
        gradient.setColorAt(qBound(0.0, center - 0.14, 1.0), lineBase);
        gradient.setColorAt(qBound(0.0, center, 1.0), lineHighlight);
        gradient.setColorAt(qBound(0.0, center + 0.14, 1.0), lineBase);
        gradient.setColorAt(1.0, lineBase);
        return QBrush(gradient);
    };

    painter->setBrush(lineBrush(line));
    painter->drawRoundedRect(line, 4, 4);
    painter->restore();
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
