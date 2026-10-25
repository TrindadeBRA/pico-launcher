#include "common.h"
#include <string.h>
#include "core/StringUtil.h"
#include "FileInfo.h"

FileInfo::FileInfo(const FileInfo& fileInfo)
    : _type(fileInfo._type), _fastFileRef(fileInfo._fastFileRef), _attributes(fileInfo._attributes)
{
    if (fileInfo.GetAbsolutePath()) SetAbsolutePath(fileInfo.GetAbsolutePath());
    u32 bufferLength = strlen(fileInfo.GetFileName()) + 1;
    _name = std::make_unique_for_overwrite<TCHAR[]>(bufferLength);
    StringUtil::Copy(_name.get(), fileInfo.GetFileName(), bufferLength);
}

FileInfo::FileInfo(const TCHAR* fileName, const FileType* type, const FastFileRef& fastFileRef, u8 attributes)
    : _type(type), _fastFileRef(fastFileRef), _attributes(attributes)
{
    u32 bufferLength = strlen(fileName) + 1;
    _name = std::make_unique_for_overwrite<TCHAR[]>(bufferLength);
    StringUtil::Copy(_name.get(), fileName, bufferLength);
}
void FileInfo::SetAbsolutePath(const char* path)
{
    const auto length = strlen(path) + 1;
    _absolutePath = std::make_unique<char[]>(length);
    StringUtil::Copy(_absolutePath.get(), path, length);
}
