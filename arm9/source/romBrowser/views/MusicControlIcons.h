#pragma once
#include <array>

// Code-native 16x16 4bpp glyphs in DS tile order.
constexpr std::array<unsigned, 32> MakeMusicIcon(const std::array<unsigned short, 16>& rows)
{
    std::array<unsigned, 32> tiles{};
    for (unsigned y = 0; y < 16; ++y)
        for (unsigned x = 0; x < 16; ++x)
            if (rows[y] & (1u << x))
                tiles[(y / 8 * 2 + x / 8) * 8 + y % 8] |= 15u << ((x % 8) * 4);
    return tiles;
}
constexpr auto VolumeDownIcon = MakeMusicIcon({0,0,0,0,0,0,0x1ff8,0x1ff8,0,0,0,0,0,0,0,0});
constexpr auto VolumeUpIcon = MakeMusicIcon({0,0,0,0x0180,0x0180,0x0180,0x1ff8,0x1ff8,0x0180,0x0180,0x0180,0,0,0,0,0});
constexpr auto NextTrackIcon = MakeMusicIcon({0,0,0x1008,0x1018,0x1038,0x1078,0x10f8,0x11f8,0x10f8,0x1078,0x1038,0x1018,0x1008,0,0,0});
