#pragma once
#include <memory>
#include "IAudioStreamPlayer.h"
#include "services/settings/IAppSettingsService.h"
#include "rng/RandomGenerator.h"
#include "IBgmService.h"

/// @brief Class implementing a background music service.
class BgmService : public IBgmService
{
public:
    constexpr BgmService(
        std::unique_ptr<IAudioStreamPlayer> audioStreamPlayer,
        IAppSettingsService& appSettingsService,
        RandomGenerator& randomGenerator)
        : _audioStreamPlayer(std::move(audioStreamPlayer))
        , _appSettingsService(appSettingsService)
        , _randomGenerator(randomGenerator) { }

    bool StartBgm(const TCHAR* filePath) override;
    bool StartBgmFromConfig() override;
    void StopBgm() override;
    void SetVolume(unsigned percent) override { _audioStreamPlayer->SetVolume(percent); }
    bool NextBgm() override;

private:
    std::unique_ptr<IAudioStreamPlayer> _audioStreamPlayer;
    IAppSettingsService& _appSettingsService;
    RandomGenerator& _randomGenerator;
    String<char, 256> _currentTrack;
    bool PlayFromTheme(bool next);
};
