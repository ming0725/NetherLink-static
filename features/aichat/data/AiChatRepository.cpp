#include "AiChatRepository.h"

#include <QRandomGenerator>
#include <QStringList>

#include <algorithm>

#include "shared/data/RepositoryTemplate.h"

namespace {

class AiChatListRequestOperation final
    : public RepositoryTemplate<AiChatListRequest, QVector<AiChatListEntry>> {
public:
    explicit AiChatListRequestOperation(QVector<AiChatListEntry> entries)
        : m_entries(std::move(entries))
    {
    }

private:
    QVector<AiChatListEntry> doRequest(const AiChatListRequest&) const override
    {
        return m_entries;
    }

    void onAfterRequest(const AiChatListRequest&, QVector<AiChatListEntry>& result) const override
    {
        std::sort(result.begin(), result.end(), [](const AiChatListEntry& lhs, const AiChatListEntry& rhs) {
            return lhs.time > rhs.time;
        });
    }

    QVector<AiChatListEntry> m_entries;
};

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
        m_entries.push_back({
                QStringLiteral("ai-chat-%1").arg(m_nextConversationId++),
                QStringLiteral("%1 %2")
                        .arg(sampleTitles.at(i % sampleTitles.size()))
                        .arg(i + 1),
                now.addDays(-offsetDays).addSecs(-offsetMinutes * 60)
        });
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
    return AiChatListRequestOperation(m_entries).request(query);
}

QString AiChatRepository::createAiChatConversation(const QString& title, const QDateTime& time)
{
    const QString trimmedTitle = title.trimmed();
    if (trimmedTitle.isEmpty() || !time.isValid()) {
        return {};
    }

    QMutexLocker locker(&m_mutex);
    const QString conversationId = QStringLiteral("ai-chat-%1").arg(m_nextConversationId++);
    m_entries.push_back({conversationId, trimmedTitle, time});
    return conversationId;
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
    return true;
}
