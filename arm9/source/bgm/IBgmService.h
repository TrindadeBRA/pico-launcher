#pragma once
#include "fat/ff.h"

/// @brief Interface for a background music service.
class IBgmService
{
public:
    virtual ~IBgmService() = 0;

    /// @brief Starts playback of the given file.
    /// @param filePath The file to play.
    /// @return True if playback was successfully started, or false otherwise.
    virtual bool StartBgm(const TCHAR* filePath) = 0;

    /// @brief Starts playback of the background music according to the app config.
    virtual bool StartBgmFromConfig() = 0;

    /// @brief If currently playing, stops playback.
    virtual void StopBgm() = 0;
    virtual void SetVolume(unsigned percent) = 0;
    virtual bool NextBgm() = 0;
};

inline IBgmService::~IBgmService() { }
