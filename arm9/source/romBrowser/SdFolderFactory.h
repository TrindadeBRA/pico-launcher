#pragma once
#include <memory>
#include <string>
#include <vector>
#include "SdFolder.h"
#include "FileType/IFileTypeProvider.h"

class SdFolderFactory
{
public:
    SdFolderFactory(const IFileTypeProvider* fileTypeProvider)
        : _fileTypeProvider(fileTypeProvider) { }

    std::unique_ptr<SdFolder> CreateFromPath(const char* path) const;
    std::unique_ptr<SdFolder> CreateFromFiles(const std::vector<std::string>& paths) const;

private:
    const IFileTypeProvider* _fileTypeProvider;
};