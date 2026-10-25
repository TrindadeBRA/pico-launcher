#include "common.h"
#include "fat/File.h"
#include "json/ArduinoJson.h"
#include "FavoritesRepository.h"

static constexpr const char* Path = "/_pico/favorites.json";
static constexpr const char* Backup = "/_pico/favorites.bak";
static constexpr const char* Temporary = "/_pico/favorites.tmp";

bool FavoritesRepository::Read(const char* path, FavoritePaths& result) const
{
    auto file = std::make_unique<File>();
    if (file->Open(path, FA_READ) != FR_OK) return false;
    const auto size = file->GetSize();
    if (size == 0 || size > 256 * 1024) return false;
    auto data = std::unique_ptr<char[]>(new(cache_align) char[size]);
    if (!file->ReadExact(data.get(), size)) return false;
    DynamicJsonDocument json(size * 2 + 1024);
    if (deserializeJson(json, data.get(), size) != DeserializationError::Ok
        || !json["favorites"].is<JsonArray>()) return false;
    for (auto entry : json["favorites"].as<JsonArrayConst>())
    {
        if (entry.is<const char*>()) result.Add(entry.as<const char*>());
    }
    return true;
}

void FavoritesRepository::Load()
{
    FavoritePaths loaded;
    if (Read(Path, loaded)) _favorites = std::move(loaded);
    else
    {
        FavoritePaths recovered;
        if (Read(Backup, recovered)) _favorites = std::move(recovered);
    }
}

bool FavoritesRepository::Save(const FavoritePaths& favorites) const
{
    size_t capacity = 1024;
    for (const auto& path : favorites.GetPaths()) capacity += path.size() + 64;
    DynamicJsonDocument json(capacity);
    auto array = json.createNestedArray("favorites");
    for (const auto& path : favorites.GetPaths()) array.add(path.c_str());
    if (json.overflowed()) return false;
    const u32 size = measureJson(json);
    auto data = std::unique_ptr<char[]>(new(cache_align) char[size + 1]);
    serializeJson(json, data.get(), size + 1);
    auto file = std::make_unique<File>();
    if (file->Open(Temporary, FA_WRITE | FA_CREATE_ALWAYS) != FR_OK) return false;
    u32 written = 0;
    if (file->Write(data.get(), size, written) != FR_OK || written != size
        || file->Sync() != FR_OK) return false;
    if (file->Close() != FR_OK) return false;

    // Keep a recoverable previous version if power is lost during replacement.
    FILINFO info;
    bool rotatedOriginal = false;
    if (f_stat(Path, &info) == FR_OK)
    {
        FavoritePaths previous;
        if (Read(Path, previous))
        {
            auto removed = f_unlink(Backup);
            if (removed != FR_OK && removed != FR_NO_FILE) return false;
            if (f_rename(Path, Backup) != FR_OK) return false;
            rotatedOriginal = true;
        }
        else if (f_unlink(Path) != FR_OK) return false; // Preserve a valid recovery backup.
    }
    if (f_rename(Temporary, Path) != FR_OK)
    {
        if (rotatedOriginal) f_rename(Backup, Path);
        return false;
    }
    return true;
}

bool FavoritesRepository::Toggle(const char* path)
{
    auto updated = _favorites;
    if (!updated.Toggle(path) || !Save(updated)) return false;
    _favorites = std::move(updated);
    return true;
}
