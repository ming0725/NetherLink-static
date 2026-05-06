#include "AudioService.h"

#include <QSoundEffect>
#include <QUrl>

namespace {

constexpr int kPortalPoolSize = 6;
constexpr float kPortalVolume = 0.75f;
const QUrl kPortalSource(QStringLiteral("qrc:/resources/audio/portal.wav"));

} // namespace

AudioService& AudioService::instance()
{
    static AudioService service;
    return service;
}

AudioService::AudioService(QObject* parent)
    : QObject(parent)
{
}

void AudioService::preloadUiSounds()
{
    ensurePortalPool();
}

void AudioService::playPortalClick()
{
    ensurePortalPool();

    QSoundEffect* sound = idlePortalSound();
    if (!sound || !sound->isLoaded() || sound->status() == QSoundEffect::Error) {
        return;
    }

    sound->play();
}

void AudioService::ensurePortalPool()
{
    if (!m_portalPool.isEmpty()) {
        return;
    }

    m_portalPool.reserve(kPortalPoolSize);
    for (int i = 0; i < kPortalPoolSize; ++i) {
        auto* sound = new QSoundEffect(this);
        sound->setSource(kPortalSource);
        sound->setLoopCount(1);
        sound->setVolume(kPortalVolume);
        m_portalPool.append(sound);
    }
}

QSoundEffect* AudioService::idlePortalSound()
{
    if (m_portalPool.isEmpty()) {
        return nullptr;
    }

    const int poolSize = m_portalPool.size();
    for (int offset = 0; offset < poolSize; ++offset) {
        const int index = (m_nextPortalIndex + offset) % poolSize;
        QSoundEffect* sound = m_portalPool.at(index);
        if (sound && !sound->isPlaying()) {
            m_nextPortalIndex = (index + 1) % poolSize;
            return sound;
        }
    }

    return nullptr;
}
