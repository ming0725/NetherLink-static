#include "AiChatStreamClient.h"

#include <QRandomGenerator>
#include <QStringList>
#include <QTimer>
#include <QtGlobal>

AiChatStreamClient::AiChatStreamClient(QObject* parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &AiChatStreamClient::emitNextChunk);
}

bool AiChatStreamClient::isRunning() const
{
    return m_running;
}

void AiChatStreamClient::start(const QString& prompt)
{
    cancel();
    m_response = responseForPrompt(prompt);
    m_offset = 0;
    m_running = !m_response.isEmpty();
    if (m_running) {
        scheduleNextChunk(160, 320);
    }
}

void AiChatStreamClient::cancel()
{
    m_timer->stop();
    m_response.clear();
    m_offset = 0;
    m_running = false;
}

void AiChatStreamClient::emitNextChunk()
{
    if (!m_running) {
        return;
    }

    if (m_offset >= m_response.size()) {
        cancel();
        emit finished();
        return;
    }

    const int chunkSize = QRandomGenerator::global()->bounded(2, 8);
    const int nextOffset = qMin(m_offset + chunkSize, m_response.size());
    const QString chunk = m_response.mid(m_offset, nextOffset - m_offset);
    m_offset = nextOffset;
    emit chunkReceived(chunk);

    if (m_offset >= m_response.size()) {
        cancel();
        emit finished();
        return;
    }

    scheduleNextChunk();
}

QString AiChatStreamClient::responseForPrompt(const QString& prompt) const
{
    const QString promptPreview = prompt.simplified().left(80);
    const QStringList replies = {
        QStringLiteral("### 本地流式接口\n\n这次回复来自一个异步 stream client，不会提前把完整内容交给界面层。\n\n你刚才输入的是：**%1**。\n\n当前链路按真实接口的方式工作：请求开始后只持续收到增量 chunk，界面把每个 chunk 追加到同一个 AI 气泡。")
                .arg(promptPreview),
        QStringLiteral("### 增量回复测试\n\n流式请求已经建立，下面的文本会以网络分片形式陆续返回。\n\n> 输入摘要：%1\n\n验证重点：\n\n1. 发送期间不能再提交新消息\n2. 停止按钮只取消后续传输\n3. 已经显示的内容不会被补齐或覆盖")
                .arg(promptPreview),
        QStringLiteral("收到。这里走的是可替换网络接口的流式输出：\n\n```text\nrequest started -> chunk received -> append to message bubble -> request finished\n```\n\n压力样例：**%1**。\n\n接真实接口时保留 messageId 定位和 append 逻辑即可。")
                .arg(promptPreview),
        QStringLiteral("### 输入体验验证\n\nAI 正在回复时，右下角图标会切换为离线图标，点击后立即停止传输。\n\n关于你的输入：**%1**。\n\n空闲时图标恢复为书写图标，可以再次发送新的消息。")
                .arg(promptPreview)
    };

    return replies.at(QRandomGenerator::global()->bounded(replies.size()));
}

void AiChatStreamClient::scheduleNextChunk(int minDelayMs, int maxDelayMs)
{
    const int lower = qMax(1, minDelayMs);
    const int upper = qMax(lower + 1, maxDelayMs);
    m_timer->start(QRandomGenerator::global()->bounded(lower, upper));
}
