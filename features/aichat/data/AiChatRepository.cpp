#include "AiChatRepository.h"

#include <QRandomGenerator>
#include <QStringList>

#include <algorithm>

namespace {

QString normalizedMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text.trimmed();
}

QString normalizedAiMessageText(QString text)
{
    text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    return text;
}

} // namespace

AiChatRepository::AiChatRepository(QObject* parent)
    : QObject(parent)
{
    const QStringList sampleTitles = {
        QStringLiteral("深度研究助理"),
        QStringLiteral("界面重构草案"),
        QStringLiteral("插件调试记录"),
        QStringLiteral("多代理协作实验"),
        QStringLiteral("日报自动整理"),
        QStringLiteral("提示词优化备份"),
        QStringLiteral("模型切换对照"),
        QStringLiteral("前端交互验证")
    };

    const QDateTime now = QDateTime::currentDateTime();
    m_entries.reserve(40);
    for (int i = 0; i < 40; ++i) {
        const int offsetDays = QRandomGenerator::global()->bounded(0, 18);
        const int offsetMinutes = QRandomGenerator::global()->bounded(0, 24 * 60);
        AiChatListEntry entry {
                QStringLiteral("ai-chat-%1").arg(m_nextConversationId++),
                QStringLiteral("%1 %2")
                        .arg(sampleTitles.at(i % sampleTitles.size()))
                        .arg(i + 1),
                now.addDays(-offsetDays).addSecs(-offsetMinutes * 60)
        };
        m_entries.push_back(entry);
        appendInitialMessages(entry, i);
    }
}

AiChatRepository& AiChatRepository::instance()
{
    static AiChatRepository repo;
    return repo;
}

QVector<AiChatListEntry> AiChatRepository::requestAiChatList(const AiChatListRequest& query) const
{
    QMutexLocker locker(&m_mutex);
    if (query.limit <= 0 || query.offset < 0 || query.offset >= m_entries.size()) {
        return {};
    }

    QVector<AiChatListEntry> sortedEntries = m_entries;
    std::sort(sortedEntries.begin(), sortedEntries.end(), [](const AiChatListEntry& lhs, const AiChatListEntry& rhs) {
        return lhs.time > rhs.time;
    });

    const int offset = qBound(0, query.offset, sortedEntries.size());
    const int limit = qMax(0, query.limit);
    return sortedEntries.mid(offset, limit);
}

QVector<AiChatMessage> AiChatRepository::requestAiChatMessages(const QString& conversationId) const
{
    QMutexLocker locker(&m_mutex);
    return m_messages.value(conversationId);
}

QString AiChatRepository::createAiChatConversation(const QString& title, const QDateTime& time)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty() || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const QString conversationId = QStringLiteral("ai-chat-%1").arg(m_nextConversationId++);
    const AiChatListEntry entry {conversationId, trimmedTitle, time};
    m_entries.push_back(entry);
    m_messages.insert(conversationId, {
            {
                    QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
                    conversationId,
                    QStringLiteral("**你好，我是 AI 助手。**\n\n可以直接在下方输入内容开始对话。"),
                    false,
                    time
            }
    });
    return conversationId;
}

AiChatMessage AiChatRepository::addAiChatMessage(const QString& conversationId,
                                                 const QString& text,
                                                 bool isFromUser,
                                                 const QDateTime& time)
{
    const QString messageText = isFromUser ? normalizedMessageText(text) : normalizedAiMessageText(text);
    if (conversationId.isEmpty() || (isFromUser && messageText.isEmpty()) || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    auto entryIt = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
        return entry.conversationId == conversationId;
    });
    if (entryIt == m_entries.end()) {
        return {};
    }

    AiChatMessage message {
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            conversationId,
            messageText,
            isFromUser,
            time
    };
    m_messages[conversationId].push_back(message);
    entryIt->time = time;
    if (isFromUser) {
        entryIt->title = messageText.simplified().left(28);
    }
    return message;
}

bool AiChatRepository::updateAiChatMessageText(const QString& conversationId,
                                               const QString& messageId,
                                               const QString& text,
                                               const QDateTime& time)
{
    const QString messageText = normalizedAiMessageText(text);
    if (conversationId.isEmpty() || messageId.isEmpty() || !time.isValid()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    auto messagesIt = m_messages.find(conversationId);
    if (messagesIt == m_messages.end()) {
        return false;
    }

    for (AiChatMessage& message : messagesIt.value()) {
        if (message.messageId == messageId) {
            message.text = messageText;
            message.time = time;
            for (AiChatListEntry& entry : m_entries) {
                if (entry.conversationId == conversationId) {
                    entry.time = time;
                    break;
                }
            }
            return true;
        }
    }

    return false;
}

bool AiChatRepository::renameAiChatConversation(const QString& conversationId, const QString& title)
{
    const QString trimmedTitle = title.trimmed();
    if (conversationId.isEmpty() || trimmedTitle.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    for (AiChatListEntry& entry : m_entries) {
        if (entry.conversationId == conversationId) {
            entry.title = trimmedTitle;
            return true;
        }
    }
    return false;
}

bool AiChatRepository::removeAiChatConversation(const QString& conversationId)
{
    if (conversationId.isEmpty()) {
        return false;
    }

    QMutexLocker locker(&m_mutex);
    const auto it = std::find_if(m_entries.begin(), m_entries.end(), [&conversationId](const AiChatListEntry& entry) {
        return entry.conversationId == conversationId;
    });
    if (it == m_entries.end()) {
        return false;
    }

    m_entries.erase(it);
    m_messages.remove(conversationId);
    return true;
}

void AiChatRepository::appendInitialMessages(const AiChatListEntry& entry, int sampleIndex)
{
    const QStringList prompts = {
        QStringLiteral("帮我把这段需求拆成可执行的实现步骤。"),
        QStringLiteral("分析一下这个界面交互有没有明显问题。"),
        QStringLiteral("给出一个更稳妥的重构方案。"),
        QStringLiteral("整理一下今天的调试结论。")
    };
    const QStringList replies = {
        QStringLiteral("可以。建议按这个顺序处理：\n\n- 先确认 **数据边界**\n- 再拆开绘制、交互和持久化\n- 列表层保持虚拟化，`delegate` 只负责可见项绘制。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。"),
        QStringLiteral("主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。"),
        QStringLiteral("如果已经有基于 `QListView` 的滚动容器，可以继续复用。\n\n新增类只承接：\n\n- AI 气泡绘制\n- 字符级命中测试\n- 复制逻辑。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。"),
        QStringLiteral("**结论：**右侧消息区域应以模型驱动。\n\n输入框复用现有组件；后续接真实 AI 接口时，只替换 `repository` 层即可。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。主要风险有两点：\n\n1. 滚动性能\n2. 文字选择状态\n\n选择状态建议保存在 `view/delegate` 中，不写回消息数据，避免刷新时污染模型。")
    };

    QVector<AiChatMessage>& messages = m_messages[entry.conversationId];
    const QDateTime firstTime = entry.time.addSecs(-180);
    messages.push_back({
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            entry.conversationId,
            prompts.at(sampleIndex % prompts.size()),
            true,
            firstTime
    });
    messages.push_back({
            QStringLiteral("ai-message-%1").arg(m_nextMessageId++),
            entry.conversationId,
            replies.at(sampleIndex % replies.size()),
            false,
            entry.time
    });
}
