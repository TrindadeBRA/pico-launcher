#pragma once
#include <vector>
#include "../IRomBrowserController.h"
#include "services/settings/RomBrowserDisplaySettings.h"

/// @brief View model for the display settings screen.
class DisplaySettingsViewModel
{
public:
    explicit DisplaySettingsViewModel(IRomBrowserController* romBrowserController)
        : _romBrowserController(romBrowserController)
        , _romBrowserDisplaySettings(_romBrowserController->GetRomBrowserDisplaySettings()) { }

    constexpr RomBrowserLayout GetRomBrowserDisplayMode() const
    {
        return _romBrowserDisplaySettings.layout;
    }

    void SetRomBrowserDisplayMode(RomBrowserLayout romBrowserDisplayMode)
    {
        if (_romBrowserDisplaySettings.layout != romBrowserDisplayMode)
        {
            _romBrowserDisplaySettings.layout = romBrowserDisplayMode;
            _romBrowserController->SetRomBrowserDisplaySettings(_romBrowserDisplaySettings);
        }
    }

    constexpr RomBrowserSortMode GetRomBrowserSortMode() const
    {
        return _romBrowserDisplaySettings.sortMode;
    }

    void SetRomBrowserSortMode(RomBrowserSortMode romBrowserSortMode)
    {
        if (_romBrowserDisplaySettings.sortMode != romBrowserSortMode)
        {
            _romBrowserDisplaySettings.sortMode = romBrowserSortMode;
            _romBrowserController->SetRomBrowserDisplaySettings(_romBrowserDisplaySettings);
        }
    }

    void RefreshThemes();
    void CycleTheme(int direction);
    const char* GetThemeName() const { return _romBrowserController->GetThemeName(); }
    bool HasThemes() const { return !_themeNames.empty(); }

    void Close()
    {
        _romBrowserController->HideDisplaySettings();
    }

private:
    std::vector<String<char, 64>> _themeNames;
    IRomBrowserController* _romBrowserController;
    RomBrowserDisplaySettings _romBrowserDisplaySettings;
};
