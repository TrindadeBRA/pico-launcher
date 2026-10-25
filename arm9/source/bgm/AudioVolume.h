#pragma once
#include <algorithm>
#include <cstdint>

constexpr unsigned ClampMusicVolume(int percent)
{
    return static_cast<unsigned>(std::clamp(percent, 0, 100));
}

constexpr std::int16_t ScaleMusicSample(std::int16_t sample, unsigned percent)
{
    return static_cast<std::int16_t>(static_cast<int>(sample) * static_cast<int>(std::min(percent, 100u)) / 100);
}
