#pragma once
#include "fat/ff.h"
#include "fat/FastFileRef.h"
#include "FileType/FileType.h"

class InternalFileInfo;

class FileInfo
{
public:
    FileInfo() { }
    FileInfo(const FileInfo& fileInfo);
    FileInfo(const TCHAR* fileName, const FileType* type, const FastFileRef& fastFileRef, u8 attributes);

    FileInfo &operator=(FileInfo&& rhs)
    {
        if (this != &rhs)
        {
            _name = std::move(rhs._name);
            _absolutePath = std::move(rhs._absolutePath);
            _attributes = rhs._attributes;
            _type = rhs._type;
            _fastFileRef = rhs._fastFileRef;
        }

        return *this;
    }

    void SetAbsolutePath(const char* path);
    const char* GetAbsolutePath() const { return _absolutePath.get(); }

    const TCHAR* GetFileName() const { return _name.get(); }
    const FileType* GetFileType() const { return _type; }
    u32 GetFileSize() const { return _fastFileRef.GetFileSize(); }

    InternalFileInfo* CreateInternalFileInfo() const
    {
        return _type->CreateInternalFileInfo(_fastFileRef);
    }

    const FastFileRef& GetFastFileRef() const { return _fastFileRef; }

    bool IsReadOnly() const { return _attributes & AM_RDO; }
    bool IsHidden() const { return _attributes & AM_HID; }
    bool IsSystem() const { return _attributes & AM_SYS; }

private:
    std::unique_ptr<TCHAR[]> _name;
    std::unique_ptr<char[]> _absolutePath;
    const FileType* _type;
    FastFileRef _fastFileRef;
    u8 _attributes;
};
