#include "common.h"
#include <vector>
#include "romBrowser/FileInfo.h"
#include "core/mini-printf.h"
#include "Pcm16FileAudioStream.h"
#include "BcstmAudioStream.h"
#include "romBrowser/SdFolder.h"
#include "romBrowser/SdFolderFactory.h"
#include "romBrowser/FileType/NullFileTypeProvider.h"
#include "BgmService.h"

bool BgmService::StartBgm(const TCHAR* filePath)
{
    auto stream = std::make_unique<BcstmAudioStream>();
    if (!stream->Open(filePath))
        return false;

    return _audioStreamPlayer->StartPlayback(std::move(stream));
}

bool BgmService::StartBgmFromConfig()
{
    const auto& settings = _appSettingsService.GetAppSettings();
    SetVolume(settings.musicVolume);
    if (!settings.musicEnabled) { StopBgm(); return true; }
    const bool started = PlayFromTheme(false);
    if (!started) StopBgm();
    return started;
}

bool BgmService::NextBgm()
{
    if (!_appSettingsService.GetAppSettings().musicEnabled) return false;
    SetVolume(_appSettingsService.GetAppSettings().musicVolume);
    return PlayFromTheme(true);
}

bool BgmService::PlayFromTheme(bool next)
{
    char folder[128];
    mini_snprintf(folder, sizeof(folder), "/_pico/themes/%s/bgm", _appSettingsService.GetAppSettings().theme.GetString());
    NullFileTypeProvider fileTypeProvider;
    auto bgmFolder = SdFolderFactory(&fileTypeProvider).CreateFromPath(folder);
    if (!bgmFolder) return false;
    bgmFolder->SortByNameInPlace();
    std::vector<const FileInfo*> tracks;
    for (int i = 0; i < bgmFolder->GetFileCount(); ++i)
    {
        auto file = bgmFolder->GetFiles()[i];
        auto extension = strrchr(file->GetFileName(), '.');
        if (file->GetFileType()->GetClassification() != FileTypeClassification::Folder
            && !file->IsHidden() && file->GetFileName()[0] != '.'
            && extension && strcasecmp(extension, ".bcstm") == 0)
            tracks.push_back(file);
    }
    if (tracks.empty()) return false;
    unsigned first = _randomGenerator.NextU32(tracks.size());
    if (next)
    {
        for (unsigned i = 0; i < tracks.size(); ++i)
        {
            char path[256];
            mini_snprintf(path, sizeof(path), "%s/%s", folder, tracks[i]->GetFileName());
            if (strcasecmp(path, _currentTrack) == 0) { first = (i + 1) % tracks.size(); break; }
        }
    }
    for (unsigned offset = 0; offset < tracks.size(); ++offset)
    {
        auto file = tracks[(first + offset) % tracks.size()];
        auto stream = std::make_unique<BcstmAudioStream>();
        if (!stream->Open(file->GetFastFileRef())) continue;
        if (!_audioStreamPlayer->StartPlayback(std::move(stream))) continue;
        char path[256];
        mini_snprintf(path, sizeof(path), "%s/%s", folder, file->GetFileName());
        _currentTrack = path;
        return true;
    }
    return false;
}

void BgmService::StopBgm()
{
    _audioStreamPlayer->StopPlayback();
}
