#pragma once

#include <QObject>

#include <memory>

struct AudioServicePrivate;

class AudioService : public QObject {
    Q_OBJECT

public:
    enum class SoundEffect {
        ButtonClick,
        Portal,
        SuccessfulHit,
        Break,
        ClassicHurt,
        ChestOpen,
        ChestClosed,
        Fuse
    };

    static AudioService& instance();
    ~AudioService() override;

    void preloadUiSounds();
    void play(SoundEffect effect);
    void playButtonClick();
    void playPortalClick();

    int masterVolume() const;
    int soundEffectVolume() const;
    void setMasterVolume(int volume);
    void setSoundEffectVolume(int volume);

private:
    explicit AudioService(QObject* parent = nullptr);
    AudioService(const AudioService&) = delete;
    AudioService& operator=(const AudioService&) = delete;

    std::unique_ptr<AudioServicePrivate> d;
};
