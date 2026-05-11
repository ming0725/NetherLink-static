#pragma once

#include <QObject>
#include <QString>

class QTimer;

class AiChatStreamClient : public QObject
{
    Q_OBJECT

public:
    explicit AiChatStreamClient(QObject* parent = nullptr);

    bool isRunning() const;
    void start(const QString& prompt);
    void cancel();

signals:
    void chunkReceived(const QString& chunk);
    void finished();

private:
    void emitNextChunk();
    QString responseForPrompt(const QString& prompt) const;
    void scheduleNextChunk(int minDelayMs = 35, int maxDelayMs = 120);

    QTimer* m_timer = nullptr;
    QString m_response;
    int m_offset = 0;
    bool m_running = false;
};
