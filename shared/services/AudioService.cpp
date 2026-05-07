#include "AudioService.h"

#include <miniaudio.h>

#include <QFile>
#include <QMutex>
#include <QMutexLocker>

#include <memory>
#include <iterator>
#include <vector>

namespace {

struct SoundDefinition {
    AudioService::SoundEffect effect;
    const char* resourcePath;
    float volume;
    int poolSize;
};

constexpr SoundDefinition kSoundDefinitions[] = {
    {AudioService::SoundEffect::ButtonClick, ":/resources/audio/click.wav", 0.80f, 24},
    {AudioService::SoundEffect::Portal, ":/resources/audio/portal.wav", 0.75f, 12},
    {AudioService::SoundEffect::SuccessfulHit, ":/resources/audio/successful_hit.wav", 0.80f, 8},
    {AudioService::SoundEffect::Break, ":/resources/audio/break.wav", 0.80f, 8},
    {AudioService::SoundEffect::ClassicHurt, ":/resources/audio/classic_hurt.wav", 0.80f, 8},
    {AudioService::SoundEffect::ChestOpen, ":/resources/audio/chestopen.wav", 0.80f, 8},
    {AudioService::SoundEffect::ChestClosed, ":/resources/audio/chestclosed.wav", 0.80f, 8},
    {AudioService::SoundEffect::Fuse, ":/resources/audio/fuse.wav", 0.80f, 8},
};

struct AudioVoice {
    ma_decoder decoder {};
    ma_sound sound {};
    bool decoderInitialized = false;
    bool soundInitialized = false;
};

struct AudioClip {
    explicit AudioClip(const SoundDefinition& definition)
        : effect(definition.effect)
        , resourcePath(QString::fromUtf8(definition.resourcePath))
        , volume(definition.volume)
        , poolSize(definition.poolSize)
    {
    }

    AudioService::SoundEffect effect;
    QString resourcePath;
    float volume = 1.0f;
    int poolSize = 1;
    QByteArray encodedData;
    std::vector<std::unique_ptr<AudioVoice>> voices;
    int nextVoice = 0;
    bool loaded = false;
};

} // namespace

struct AudioServicePrivate {
    QMutex mutex;
    ma_engine engine {};
    bool engineInitialized = false;
    std::vector<std::unique_ptr<AudioClip>> clips;

    AudioServicePrivate()
    {
        clips.reserve(std::size(kSoundDefinitions));
        for (const SoundDefinition& definition : kSoundDefinitions) {
            clips.push_back(std::make_unique<AudioClip>(definition));
        }
    }

    ~AudioServicePrivate()
    {
        QMutexLocker locker(&mutex);

        for (const std::unique_ptr<AudioClip>& clip : clips) {
            for (const std::unique_ptr<AudioVoice>& voice : clip->voices) {
                if (voice->soundInitialized) {
                    ma_sound_uninit(&voice->sound);
                }
                if (voice->decoderInitialized) {
                    ma_decoder_uninit(&voice->decoder);
                }
            }
        }
        clips.clear();

        if (engineInitialized) {
            ma_engine_uninit(&engine);
            engineInitialized = false;
        }
    }

    bool ensureEngine()
    {
        if (engineInitialized) {
            return true;
        }

        ma_engine_config config = ma_engine_config_init();
        if (ma_engine_init(&config, &engine) != MA_SUCCESS) {
            return false;
        }

        engineInitialized = true;
        return true;
    }

    AudioClip* clipFor(AudioService::SoundEffect effect)
    {
        for (const std::unique_ptr<AudioClip>& clip : clips) {
            if (clip->effect == effect) {
                return clip.get();
            }
        }
        return nullptr;
    }

    bool ensureClip(AudioClip& clip)
    {
        if (clip.loaded) {
            return true;
        }
        if (!ensureEngine()) {
            return false;
        }

        QFile file(clip.resourcePath);
        if (!file.open(QIODevice::ReadOnly)) {
            return false;
        }

        clip.encodedData = file.readAll();
        if (clip.encodedData.isEmpty()) {
            return false;
        }

        clip.voices.reserve(static_cast<std::size_t>(clip.poolSize));
        for (int i = 0; i < clip.poolSize; ++i) {
            auto voice = std::make_unique<AudioVoice>();

            ma_decoder_config decoderConfig = ma_decoder_config_init(ma_format_f32, 0, 0);
            ma_result result = ma_decoder_init_memory(clip.encodedData.constData(),
                                                      static_cast<std::size_t>(clip.encodedData.size()),
                                                      &decoderConfig,
                                                      &voice->decoder);
            if (result != MA_SUCCESS) {
                continue;
            }
            voice->decoderInitialized = true;

            result = ma_sound_init_from_data_source(&engine,
                                                    &voice->decoder,
                                                    MA_SOUND_FLAG_NO_SPATIALIZATION,
                                                    nullptr,
                                                    &voice->sound);
            if (result != MA_SUCCESS) {
                ma_decoder_uninit(&voice->decoder);
                voice->decoderInitialized = false;
                continue;
            }

            voice->soundInitialized = true;
            ma_sound_set_volume(&voice->sound, clip.volume);
            ma_sound_set_looping(&voice->sound, MA_FALSE);
            clip.voices.push_back(std::move(voice));
        }

        clip.loaded = !clip.voices.empty();
        return clip.loaded;
    }

    AudioVoice* nextPlayableVoice(AudioClip& clip)
    {
        if (clip.voices.empty()) {
            return nullptr;
        }

        const int voiceCount = static_cast<int>(clip.voices.size());
        for (int offset = 0; offset < voiceCount; ++offset) {
            const int index = (clip.nextVoice + offset) % voiceCount;
            AudioVoice* voice = clip.voices.at(static_cast<std::size_t>(index)).get();
            if (!ma_sound_is_playing(&voice->sound) || ma_sound_at_end(&voice->sound)) {
                clip.nextVoice = (index + 1) % voiceCount;
                return voice;
            }
        }

        AudioVoice* voice = clip.voices.at(static_cast<std::size_t>(clip.nextVoice)).get();
        clip.nextVoice = (clip.nextVoice + 1) % voiceCount;
        return voice;
    }

    void play(AudioService::SoundEffect effect)
    {
        QMutexLocker locker(&mutex);

        AudioClip* clip = clipFor(effect);
        if (!clip || !ensureClip(*clip)) {
            return;
        }

        AudioVoice* voice = nextPlayableVoice(*clip);
        if (!voice) {
            return;
        }

        ma_sound_stop(&voice->sound);
        ma_sound_seek_to_pcm_frame(&voice->sound, 0);
        ma_sound_start(&voice->sound);
    }
};

AudioService& AudioService::instance()
{
    static AudioService service;
    return service;
}

AudioService::AudioService(QObject* parent)
    : QObject(parent)
    , d(std::make_unique<AudioServicePrivate>())
{
}

AudioService::~AudioService() = default;

void AudioService::preloadUiSounds()
{
    QMutexLocker locker(&d->mutex);
    for (const std::unique_ptr<AudioClip>& clip : d->clips) {
        d->ensureClip(*clip);
    }
}

void AudioService::play(SoundEffect effect)
{
    d->play(effect);
}

void AudioService::playButtonClick()
{
    play(SoundEffect::ButtonClick);
}

void AudioService::playPortalClick()
{
    play(SoundEffect::Portal);
}
