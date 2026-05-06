#pragma once

#include <QObject>
#include <QVector>

class QSoundEffect;

class AudioService : public QObject {
    Q_OBJECT

public:
    static AudioService& instance();

    void preloadUiSounds();
    void playPortalClick();

private:
    explicit AudioService(QObject* parent = nullptr);
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    void ensurePortalPool();
    QSoundEffect* idlePortalSound();

    QVector<QSoundEffect*> m_portalPool;
    int m_nextPortalIndex = 0;
};
