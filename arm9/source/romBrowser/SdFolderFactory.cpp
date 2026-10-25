#include "common.h"
#include <vector>
#include <algorithm>
#include <strings.h>
#include "fat/Directory.h"
#include "FileInfo.h"
#include "FileType/Folder/FolderFileType.h"
#include "SdFolderFactory.h"

std::unique_ptr<SdFolder> SdFolderFactory::CreateFromPath(const char* path) const
{
    Directory directory;
    if (directory.Open(path) != FR_OK)
        return nullptr;

    int count = 0;
    int bufferSize = 8;
    auto fileInfos = (FileInfo**)malloc(sizeof(FileInfo*) * bufferSize);
    auto sdFileInfo = std::make_unique<FILINFO>();
    while (true)
    {
        if (directory.Read(sdFileInfo.get()) != FR_OK)
            return nullptr;

        if (sdFileInfo->fname[0] == 0)
            break;

        if (count >= bufferSize)
        {
            bufferSize *= 2;
            fileInfos = (FileInfo**)realloc(fileInfos, sizeof(FileInfo*) * bufferSize);
        }
        auto fileType = sdFileInfo->fattrib & AM_DIR
            ? &FolderFileType::sInstance
            : _fileTypeProvider->GetFileType(sdFileInfo->fname);
        fileInfos[count++] = new FileInfo(sdFileInfo->fname, fileType,
            FastFileRef(directory.GetFatFsDirectory(), sdFileInfo.get()), sdFileInfo->fattrib);
    }

    return std::make_unique<SdFolder>(fileInfos, count);
}
std::unique_ptr<SdFolder> SdFolderFactory::CreateFromFiles(const std::vector<std::string>& paths) const
{
    auto files = static_cast<FileInfo**>(malloc(sizeof(FileInfo*) * std::max<size_t>(paths.size(), 1)));
    int count = 0;
    auto info = std::make_unique<FILINFO>();
    for (const auto& path : paths)
    {
        const auto separator = path.find_last_of('/');
        if (separator == std::string::npos) continue;
        const auto folder = separator == 0 ? "/" : path.substr(0, separator);
        const auto name = path.substr(separator + 1);
        Directory directory;
        if (directory.Open(folder.c_str()) != FR_OK) continue;
        while (directory.Read(info.get()) == FR_OK && info->fname[0])
        {
            if (strcasecmp(info->fname, name.c_str()) != 0) continue;
            if (!(info->fattrib & (AM_DIR | AM_HID | AM_SYS)))
            {
                auto type = _fileTypeProvider->GetFileType(info->fname);
                if (type->GetClassification() == FileTypeClassification::Game)
                {
                    auto file = new FileInfo(info->fname, type,
                        FastFileRef(directory.GetFatFsDirectory(), info.get()), info->fattrib);
                    file->SetAbsolutePath(path.c_str());
                    files[count++] = file;
                }
            }
            break;
        }
    }
    return std::make_unique<SdFolder>(files, count);
}
