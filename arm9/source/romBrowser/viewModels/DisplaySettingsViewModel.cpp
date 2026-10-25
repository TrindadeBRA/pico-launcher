#include "common.h"
#include <algorithm>
#include <cstring>
#include "fat/Directory.h"
#include "themes/ThemeInfoFactory.h"
#include "DisplaySettingsViewModel.h"

void DisplaySettingsViewModel::RefreshThemes()
{
    _themeNames.clear();
    Directory directory;
    if (directory.Open("/_pico/themes") != FR_OK)
        return;

    auto info = std::make_unique<FILINFO>();
    ThemeInfoFactory factory;
    while (directory.Read(info.get()) == FR_OK && info->fname[0])
    {
        // Settings store a maximum of 64 bytes for the theme folder name.
        if (!(info->fattrib & AM_DIR) || (info->fattrib & (AM_HID | AM_SYS))
            || info->fname[0] == '.' || strlen(info->fname) > 64)
            continue;
        if (factory.CreateFromThemeFolder(info->fname))
            _themeNames.emplace_back(info->fname);
    }
    std::sort(_themeNames.begin(), _themeNames.end(),
        [](const auto& a, const auto& b) { return strcasecmp(a.GetString(), b.GetString()) < 0; });
}

void DisplaySettingsViewModel::CycleTheme(int direction)
{
    if (_themeNames.empty())
        return;
    int index = -1;
    for (unsigned i = 0; i < _themeNames.size(); ++i)
    {
        if (strcasecmp(_themeNames[i], GetThemeName()) == 0)
        {
            index = i;
            break;
        }
    }
    const int count = _themeNames.size();
    index = index < 0 ? (direction < 0 ? count - 1 : 0)
                     : (index + (direction < 0 ? count - 1 : 1)) % count;
    _romBrowserController->SetThemeName(_themeNames[index]);
}
