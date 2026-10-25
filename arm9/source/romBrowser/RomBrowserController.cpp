#include "common.h"
#include <array>
#include <algorithm>
#include "bgm/AudioVolume.h"
#include "picoLoaderBootstrap.h"
#include "PicoLoaderProcess.h"
#include "settings/SettingsProcess.h"
#include "FileType/ExtensionFileTypeProvider.h"
#include "FileType/FileType.h"
#include "SdFolderFactory.h"
#include "services/settings/IAppSettingsService.h"
#include "cheats/UsrCheatRepositoryFactory.h"
#include "cheats/EmptyCheatRepository.h"
#include "cheats/PicoLoaderCheatDataFactory.h"
#include "RomBrowserController.h"

RomBrowserController::RomBrowserController(
    IAppSettingsService* appSettingsService, TaskQueueBase* ioTaskQueue,
    TaskQueueBase* bgTaskQueue, IBgmService* bgmService)
    : _bgmService(bgmService)
    , _appSettingsService(appSettingsService)
    , _ioTaskQueue(ioTaskQueue), _bgTaskQueue(bgTaskQueue)
    , _fileTypeProvider(appSettingsService->GetAppSettings()) { }

void RomBrowserController::NavigateToPath(const TCHAR* name)
{
    StringUtil::Copy(_navigatePath, name, sizeof(_navigatePath) / sizeof(_navigatePath[0]));
    _stateMachine.Fire(RomBrowserStateTrigger::Navigate);
}

void RomBrowserController::LaunchFile(const FileInfo& fileInfo)
{
    _triggerFileInfo = FileInfo(fileInfo);
    _stateMachine.Fire(RomBrowserStateTrigger::Launch);
}

void RomBrowserController::ShowGameInfo(const FileInfo& fileInfo)
{
    _triggerFileInfo = FileInfo(fileInfo);
    _stateMachine.Fire(RomBrowserStateTrigger::ShowGameInfo);
}

void RomBrowserController::HideGameInfo()
{
    _stateMachine.Fire(RomBrowserStateTrigger::HideGameInfo);
}

void RomBrowserController::ShowDisplaySettings()
{
    _stateMachine.Fire(RomBrowserStateTrigger::ShowDisplaySettings);
}

void RomBrowserController::HideDisplaySettings()
{
    if (_saveSettingsPending)
    {
        _saveSettingsPending = false;
        _ioTaskQueue->Enqueue([this] (const vu8& cancelRequested)
        {
            _appSettingsService->Save();
            return TaskResult<void>::Completed();
        });
    }
    _stateMachine.Fire(RomBrowserStateTrigger::HideDisplaySettings);
}

void RomBrowserController::GotoSettingsScreen()
{
    HideDisplaySettings();
    _stateMachine.Fire(RomBrowserStateTrigger::GotoSettingsScreen);
}

void RomBrowserController::SetRomBrowserDisplaySettings(
    const RomBrowserDisplaySettings& romBrowserDisplaySettings)
{
    _appSettingsService->GetAppSettings().romBrowserDisplaySettings = romBrowserDisplaySettings;
    _saveSettingsPending = true;
    _stateMachine.Fire(RomBrowserStateTrigger::ChangeDisplayMode);
}

void RomBrowserController::Update()
{
    if (_musicTaskPending && _musicTask.GetTask().IsCompleted())
    {
        _musicTask.Dispose();
        _musicTaskPending = false;
        _musicNotice = _musicTaskSuccess ? "" : "Sem faixa BCSTM valida";
    }
    _stateMachine.Update();
    if (_stateMachine.HasStateChanged())
    {
        HandleTrigger();
    }
    switch (_stateMachine.GetCurrentState())
    {
        case RomBrowserState::Start:
        {
            LOG_DEBUG("RomBrowserState::Start\n");
            const auto& lastUsed = _appSettingsService->GetAppSettings().lastUsedFilePath;
            if (strlen(lastUsed.GetString()) != 0)
            {
                NavigateToPath(lastUsed.GetString());
            }
            else
            {
                NavigateToPath("/");
            }
            break;
        }
        case RomBrowserState::LoadingFolder:
        {
            if (_navigateTask.GetTask().IsCompletedSuccessfully())
            {
                _navigateTask.Dispose();
                _stateMachine.Fire(RomBrowserStateTrigger::FolderLoadDone);
            }
            break;
        }
        case RomBrowserState::Launching:
        default:
        {
            break;
        }
    }
}

void RomBrowserController::HandleTrigger()
{
    switch (_stateMachine.GetLastTrigger())
    {
        case RomBrowserStateTrigger::Navigate:
            HandleNavigateTrigger();
            break;

        case RomBrowserStateTrigger::FolderLoadDone:
            HandleFolderLoadDoneTrigger();
            break;

        case RomBrowserStateTrigger::Launch:
            HandleLaunchTrigger();
            break;

        case RomBrowserStateTrigger::ChangeDisplayMode:
            HandleChangeDisplayModeTrigger();
            break;

        case RomBrowserStateTrigger::GotoSettingsScreen:
            HandleGotoSettingsScreenTrigger();
            break;

        default:
            break;
    }
}

void RomBrowserController::HandleNavigateTrigger()
{
    LOG_DEBUG("RomBrowserStateTrigger::Navigate\n");
    auto favoritePaths = _favoritesOnly ? _favorites.GetFavorites().GetPaths() : std::vector<std::string>{};
    const bool favoritesOnly = _favoritesOnly;
    _navigateTask = _ioTaskQueue->Enqueue([this, favoritePaths = std::move(favoritePaths), favoritesOnly] (const vu8& cancelRequested)
    {
        if (!_coverRepository)
        {
            _coverRepository = std::make_unique<CoverRepository>();
            _coverRepository->Initialize();
        }
        if (!_iconRepository)
        {
            _iconRepository = std::make_unique<IconRepository>();
            _iconRepository->Initialize();
        }
        if (!_bannerRepository)
        {
            _bannerRepository = std::make_unique<BannerRepository>();
            _bannerRepository->Initialize();
        }
        if (!_cheatRepository)
        {
            _cheatRepository = UsrCheatRepositoryFactory().FromUsrCheatDat("/_pico/usrcheat.dat");
            if (!_cheatRepository)
            {
                // When usrcheat.dat is not found or cannot be read use a dummy empty cheat repository
                _cheatRepository = std::make_unique<EmptyCheatRepository>();
            }
        }

        u64 startTick = gTickCounter.GetValue();
        _navigateFileName = nullptr;
        if (strcmp(_navigatePath, "/") != 0) // can't f_stat on root dir
        {
            FILINFO fileInfo;
            if (f_stat(_navigatePath, &fileInfo) != FR_OK)
            {
                StringUtil::Copy(_navigatePath, "/", sizeof(_navigatePath) / sizeof(_navigatePath[0]));
            }
            else if (!(fileInfo.fattrib & AM_DIR))
            {
                _navigateFileName = strrchr(_navigatePath, '/') + 1;
                _navigateFileName[-1] = 0;
            }
        }
        f_chdir(_navigatePath);
        SdFolderFactory sdFolderFactory { &_fileTypeProvider };
        _newSdFolder = favoritesOnly ? sdFolderFactory.CreateFromFiles(favoritePaths)
                                    : sdFolderFactory.CreateFromPath(".");
        u64 endTick = gTickCounter.GetValue();
        LOG_DEBUG("Loading files in folder took: %d us\n", (u32)TickCounter::TicksToMicroSeconds(endTick - startTick));
        return TaskResult<void>::Completed();
    });
}

void RomBrowserController::HandleFolderLoadDoneTrigger()
{
    LOG_DEBUG("RomBrowserStateTrigger::FolderLoadDone\n");
    _romBrowserViewModel.Reset();
    _sdFolder = std::move(_newSdFolder);
    _romBrowserViewModel = SharedPtr<RomBrowserViewModel>::MakeShared(this, _navigateFileName);
}

void RomBrowserController::HandleLaunchTrigger()
{
    LOG_DEBUG("RomBrowserStateTrigger::Launch\n");
    _ioTaskQueue->Enqueue([this] (const vu8& cancelRequested)
    {
        UpdateLastUsedFilepath();
        SetPicoLoaderParams();
        LoadCheats();
        return TaskResult<void>::Completed();
    });
}

void RomBrowserController::HandleChangeDisplayModeTrigger()
{
    LOG_DEBUG("RomBrowserStateTrigger::ChangeDisplayMode\n");
    _romBrowserViewModel = SharedPtr<RomBrowserViewModel>::MakeShared(this);
}

void RomBrowserController::HandleGotoSettingsScreenTrigger()
{
    gProcessManager.Goto<SettingsProcess>();
}

void RomBrowserController::UpdateLastUsedFilepath()
{
    const auto path = GetFilePath(_triggerFileInfo);
    StringUtil::Copy(_navigatePath, path.c_str(), sizeof(_navigatePath));
    _appSettingsService->GetAppSettings().lastUsedFilePath = _navigatePath;
    _appSettingsService->Save();
}

void RomBrowserController::SetPicoLoaderParams() const
{
    auto loadParams = pload_getLoadParams();
    loadParams->savePath[0] = 0;
    loadParams->arguments[0] = 0;
    loadParams->argumentsLength = 0;
    if (_triggerFileInfo.GetFileType()->TrySetLaunchParameters(loadParams, _navigatePath))
    {
        gProcessManager.Goto<PicoLoaderProcess>();
    }
    else
    {
        LOG_FATAL("Failed to set launch parameters.\n");
    }
}

void RomBrowserController::LoadCheats() const
{
    auto cheats = _cheatRepository->GetCheatsForGame(_triggerFileInfo.GetFastFileRef());
    auto cheatData = PicoLoaderCheatDataFactory().CreateCheatData(cheats);
    pload_setCheatData(cheatData);
}

std::string RomBrowserController::GetFilePath(const FileInfo& file) const
{
    if (file.GetAbsolutePath()) return file.GetAbsolutePath();
    char cwd[256];
    if (f_getcwd(cwd, sizeof(cwd)) != FR_OK) return {};
    std::string path(cwd);
    if (path.back() != '/') path += '/';
    path += file.GetFileName();
    return path;
}

bool RomBrowserController::IsFavorite(const FileInfo& file) const
{
    return _favorites.GetFavorites().Contains(GetFilePath(file).c_str());
}

void RomBrowserController::ToggleFavorite(const FileInfo& file)
{
    if (_stateMachine.GetCurrentState() != RomBrowserState::Browser
        || file.GetFileType()->GetClassification() != FileTypeClassification::Game) return;
    const auto path = GetFilePath(file);
    if (!_favorites.Toggle(path.c_str()))
    {
        _favoriteNotice = "Falha ao salvar favoritos";
        return;
    }
    _favoriteNotice = "";
    if (_favoritesOnly) NavigateToPath(".");
}

void RomBrowserController::ToggleFavoritesView()
{
    if (_stateMachine.GetCurrentState() != RomBrowserState::Browser) return;
    _favoritesOnly = !_favoritesOnly;
    NavigateToPath(".");
}

void RomBrowserController::NavigateUp()
{
    if (_favoritesOnly)
    {
        _favoritesOnly = false;
        NavigateToPath(".");
    }
    else NavigateToPath("..");
}

void RomBrowserController::QueueMusicChange(bool next)
{
    if (_musicTaskPending) return;
    _musicNotice = "Carregando...";
    _musicTaskPending = true;
    _musicTask = _ioTaskQueue->Enqueue([this, next](const vu8& cancelRequested)
    {
        if (!IsMusicEnabled()) { _bgmService->StopBgm(); _musicTaskSuccess = true; }
        else if (next) _musicTaskSuccess = _bgmService->NextBgm();
        else _musicTaskSuccess = _bgmService->StartBgmFromConfig();
        return TaskResult<void>::Completed();
    });
}

void RomBrowserController::ToggleMusic()
{
    if (_musicTaskPending) return;
    auto& settings = _appSettingsService->GetAppSettings();
    settings.musicEnabled = !settings.musicEnabled;
    _saveSettingsPending = true;
    QueueMusicChange(false);
}

void RomBrowserController::ChangeMusicVolume(int amount)
{
    auto& settings = _appSettingsService->GetAppSettings();
    settings.musicVolume = ClampMusicVolume(static_cast<int>(settings.musicVolume) + amount);
    _bgmService->SetVolume(settings.musicVolume);
    _saveSettingsPending = true;
}

void RomBrowserController::NextMusic()
{
    if (IsMusicEnabled()) QueueMusicChange(true);
    else _musicNotice = "Ative a musica primeiro";
}
