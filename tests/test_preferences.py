"""Host tests of production favorites persistence, FAT path identity, and PCM gain.

The repository is compiled unchanged against a temporary host implementation of
File/FatFs. These tests do not emulate DS audio, graphics, or physical SD writes.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'arm9/source'

FILE_DOUBLE = r'''
#pragma once
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
using FRESULT = int;
constexpr int FR_OK=0, FR_NO_FILE=1, FR_DENIED=2, FA_READ=1, FA_WRITE=2, FA_CREATE_ALWAYS=4, FA_OPEN_EXISTING=0;
struct FILINFO {};
inline std::filesystem::path hostPath(const char* path) {
    return std::filesystem::path(std::getenv("TEST_SD_ROOT")) / (path + 1);
}
inline int f_stat(const char* path, FILINFO*) { return std::filesystem::exists(hostPath(path)) ? FR_OK : FR_NO_FILE; }
inline int f_unlink(const char* path) {
    std::error_code error;
    bool removed = std::filesystem::remove(hostPath(path), error);
    return error ? FR_DENIED : removed ? FR_OK : FR_NO_FILE;
}
inline int f_rename(const char* source, const char* target) {
    if (std::getenv("TEST_FAIL_RENAME") && std::string(source).find(".tmp") != std::string::npos) return FR_DENIED;
    if (std::filesystem::exists(hostPath(target))) return FR_DENIED;
    std::error_code error;
    std::filesystem::rename(hostPath(source), hostPath(target), error);
    return error ? FR_DENIED : FR_OK;
}
class File {
    std::fstream file;
public:
    int Open(const char* path, int flags) {
        auto mode = std::ios::binary | ((flags & FA_WRITE) ? std::ios::out : std::ios::in);
        if (flags & FA_CREATE_ALWAYS) mode |= std::ios::trunc;
        file.open(hostPath(path), mode);
        return file.is_open() ? FR_OK : FR_DENIED;
    }
    unsigned GetSize() { auto pos=file.tellg(); file.seekg(0,std::ios::end); auto size=file.tellg(); file.seekg(pos); return size; }
    int Read(void* data, unsigned size, unsigned& read) { file.read(static_cast<char*>(data),size); read=file.gcount(); return FR_OK; }
    bool ReadExact(void* data, unsigned size) { file.read(static_cast<char*>(data),size); return file.gcount()==size; }
    int Write(const void* data, unsigned size, unsigned& written) {
        if (std::getenv("TEST_FAIL_WRITE")) { written=0; return FR_DENIED; }
        file.write(static_cast<const char*>(data),size); written=file ? size : 0; return file ? FR_OK : FR_DENIED;
    }
    int Sync() { file.flush(); return file ? FR_OK : FR_DENIED; }
    int Close() { if(file.is_open()) file.close(); return FR_OK; }
};
'''

COMMON_DOUBLE = r'''
#pragma once
#include <memory>
#include <type_traits>
#include <new>
using u32=unsigned;
using u8=unsigned char;
enum CacheAlign { cache_align };
inline void* operator new[](std::size_t size, CacheAlign) { return ::operator new[](size); }
#define LOG_ERROR(...) do {} while(0)
#define LOG_DEBUG(...) do {} while(0)
'''

TEST_PROGRAM = r'''
#include <cassert>
#include <fstream>
#include "services/favorites/FavoritesRepository.h"
#include "bgm/AudioVolume.h"
#include "fat/File.h"
int main() {
    FavoritePaths list;
    assert(!list.Add(nullptr)); assert(!list.Add("relative.nds"));
    assert(!list.Add("/")); assert(!list.Add("/folder/"));
    assert(!list.Add("/a/../b.nds")); assert(!list.Add("/a/./b.nds"));
    assert(!list.Add("/a//b.nds")); assert(!list.Add(("/"+std::string(255,'a')).c_str()));
    assert(list.Add("/NDS Roms/Mario.nds"));
    assert(!list.Add("/nds roms/MARIO.nds"));
    assert(list.Add("/Another Folder/Mario.nds"));
    assert(list.GetPaths().size()==2);
    assert(list.Toggle("/NDS ROMS/mario.nds"));
    assert(!list.Contains("/NDS Roms/Mario.nds"));
    assert(list.Contains("/Another Folder/Mario.nds"));
    FavoritePaths full;
    for (unsigned i=0;i<FavoritePaths::MaxFavorites;++i) assert(full.Add(("/game"+std::to_string(i)+".nds").c_str()));
    assert(!full.Add("/one-too-many.nds"));

    FavoritesRepository repository; repository.Load();
    assert(repository.GetFavorites().GetPaths().empty());
    const char* zelda="/NDS Roms/The Legend of Zelda - Molblin's Magic Spear (BR).nds";
    assert(repository.Toggle(zelda));
    FavoritesRepository reopened; reopened.Load();
    assert(reopened.GetFavorites().Contains(zelda));
    const char* other="/Other/Mario.nds";
    setenv("TEST_FAIL_WRITE","1",1);
    assert(!reopened.Toggle(other));
    assert(!reopened.GetFavorites().Contains(other));
    unsetenv("TEST_FAIL_WRITE");
    setenv("TEST_FAIL_RENAME","1",1);
    assert(!reopened.Toggle(other));
    unsetenv("TEST_FAIL_RENAME");
    FavoritesRepository afterFailure; afterFailure.Load();
    assert(afterFailure.GetFavorites().Contains(zelda));
    assert(!afterFailure.GetFavorites().Contains(other));
    assert(afterFailure.Toggle(other));
    assert(afterFailure.Toggle(zelda));
    FavoritesRepository afterRemoval; afterRemoval.Load();
    assert(!afterRemoval.GetFavorites().Contains(zelda));
    assert(afterRemoval.GetFavorites().Contains(other));
    // Recover the last valid copy when the primary file is incomplete.
    std::ofstream(hostPath("/_pico/favorites.json")) << "{";
    FavoritesRepository recovered; recovered.Load();
    assert(recovered.GetFavorites().Contains(zelda));
    assert(recovered.GetFavorites().Contains(other));
    // A failed update after recovery must not replace the valid backup with corrupt JSON.
    setenv("TEST_FAIL_RENAME","1",1);
    assert(!recovered.Toggle("/new.nds"));
    unsetenv("TEST_FAIL_RENAME");
    FavoritesRepository recoveredAgain; recoveredAgain.Load();
    assert(recoveredAgain.GetFavorites().Contains(zelda));
    assert(recoveredAgain.GetFavorites().Contains(other));
    // Invalid and duplicate entries in a valid JSON file cannot create aliases.
    std::ofstream(hostPath("/_pico/favorites.json")) << R"({"favorites":[null,42,"relative","/a/../b","/A.nds","/a.NDS"]})";
    FavoritesRepository sanitized; sanitized.Load();
    assert(sanitized.GetFavorites().GetPaths().size()==1);
    assert(sanitized.GetFavorites().Contains("/a.nds"));

    assert(ClampMusicVolume(-10)==0); assert(ClampMusicVolume(110)==100);
    assert(ClampMusicVolume(50)==50);
    assert(ScaleMusicSample(-32768,100)==-32768);
    assert(ScaleMusicSample(32767,100)==32767);
    assert(ScaleMusicSample(-32768,50)==-16384);
    assert(ScaleMusicSample(32767,0)==0);
    assert(ScaleMusicSample(32767,110)==32767);
}
'''

class PreferencesTest(unittest.TestCase):
    def test_favorites_persistence_and_volume(self):
        with tempfile.TemporaryDirectory() as folder:
            tmp=Path(folder)
            (tmp/'fat').mkdir()
            (tmp/'fat/File.h').write_text(FILE_DOUBLE)
            (tmp/'common.h').write_text(COMMON_DOUBLE)
            (tmp/'test.cpp').write_text(TEST_PROGRAM)
            (tmp/'sd/_pico').mkdir(parents=True)
            binary=tmp/'test'
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++20','-Wall',
                '-I'+str(tmp),'-I'+str(SRC), str(tmp/'test.cpp'),
                str(SRC/'services/favorites/FavoritesRepository.cpp'),'-o',str(binary)],check=True)
            env=os.environ.copy();env['TEST_SD_ROOT']=str(tmp/'sd')
            subprocess.run([str(binary)],env=env,check=True)

    def test_settings_backward_compatibility_and_roundtrip(self):
        program = r'''
#include "common.h"
#include <cassert>
#include <fstream>
#include "fat/File.h"
#include "services/settings/AppSettings.h"
#include "services/settings/JsonAppSettingsSerializer.h"
int main() {
    JsonAppSettingsSerializer serializer;
    std::ofstream(hostPath("/_pico/settings.json")) << R"({"theme":"PiCardboard Dark","romBrowserLayout":"CoverFlow","lastUsedFilePath":"/NDS Roms/Mario.nds","fileAssociations":{"nes":{"appPath":"/emu.nds"}}})";
    AppSettings settings;
    assert(serializer.Deserialize(&settings,"/_pico/settings.json"));
    assert(settings.musicEnabled && settings.musicVolume==100);
    settings.musicEnabled=false; settings.musicVolume=30;
    serializer.Serialize(&settings,"/_pico/settings.json");
    AppSettings reopened;
    assert(serializer.Deserialize(&reopened,"/_pico/settings.json"));
    assert(!reopened.musicEnabled && reopened.musicVolume==30);
    assert(strcmp(reopened.theme,"PiCardboard Dark")==0);
    assert(reopened.romBrowserDisplaySettings.layout==RomBrowserLayout::CoverFlow);
    assert(strcmp(reopened.lastUsedFilePath,"/NDS Roms/Mario.nds")==0);
    assert(reopened.numberOfFileAssociations==1);
    assert(strcmp(reopened.fileAssociations[0].applicationPath,"/emu.nds")==0);
    std::ofstream(hostPath("/_pico/settings.json")) << R"({"musicVolume":-10,"musicEnabled":false})";
    assert(serializer.Deserialize(&reopened,"/_pico/settings.json"));
    assert(reopened.musicVolume==0 && !reopened.musicEnabled);
    std::ofstream(hostPath("/_pico/settings.json")) << R"({"musicVolume":999})";
    assert(serializer.Deserialize(&reopened,"/_pico/settings.json"));
    assert(reopened.musicVolume==100);
}
'''
        with tempfile.TemporaryDirectory() as folder:
            tmp=Path(folder); (tmp/'fat').mkdir(); (tmp/'sd/_pico').mkdir(parents=True)
            (tmp/'fat/File.h').write_text(FILE_DOUBLE)
            (tmp/'common.h').write_text(COMMON_DOUBLE)
            (tmp/'test.cpp').write_text(program)
            binary=tmp/'test'
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++20','-Wall',
                '-I'+str(tmp),'-I'+str(SRC),str(tmp/'test.cpp'),
                str(SRC/'services/settings/JsonAppSettingsSerializer.thumb.cpp'),
                str(SRC/'core/StringUtil.cpp'),'-o',str(binary)],check=True)
            env=os.environ.copy();env['TEST_SD_ROOT']=str(tmp/'sd')
            subprocess.run([str(binary)],env=env,check=True)

    def test_music_row_navigation(self):
        source=(SRC/'romBrowser/views/DisplaySettingsBottomSheetView.cpp').read_text()
        start=source.index('SharedPtr<View> DisplaySettingsBottomSheetView::MoveFocus')
        end=source.index('\nvoid DisplaySettingsBottomSheetView::SetGraphics',start)
        method=source[start:end]
        program=r'''
#include <memory>
#include <array>
#include <algorithm>
#include <cassert>
struct View {};
struct IconButton2DView : View {};
template<class T> struct SharedPtr {
    std::shared_ptr<T> pointer;
    SharedPtr() : pointer(std::make_shared<T>()) {}
    SharedPtr(std::nullptr_t) : pointer(nullptr) {}
    template<class U> requires std::is_convertible_v<U*, T*>
    SharedPtr(const SharedPtr<U>& other) : pointer(other.pointer) {}
    T* GetPointer() const { return pointer.get(); }
};
enum class FocusMoveDirection { Up,Down,Left,Right };
struct DisplaySettingsBottomSheetView {
    SharedPtr<IconButton2DView> _themeButton;
    std::array<SharedPtr<IconButton2DView>,4> _layoutOptions, _musicOptions;
    std::array<SharedPtr<IconButton2DView>,2> _sortOptions;
    SharedPtr<View> MoveFocus(const SharedPtr<View>&,FocusMoveDirection,View*);
};
'''+method+r'''
int main() {
    DisplaySettingsBottomSheetView view;
    using D=FocusMoveDirection;
    assert(view.MoveFocus(view._themeButton,D::Down,nullptr).GetPointer()==view._layoutOptions[0].GetPointer());
    assert(view.MoveFocus(view._layoutOptions[3],D::Down,nullptr).GetPointer()==view._sortOptions[1].GetPointer());
    assert(view.MoveFocus(view._sortOptions[1],D::Down,nullptr).GetPointer()==view._musicOptions[1].GetPointer());
    assert(view.MoveFocus(view._musicOptions[3],D::Up,nullptr).GetPointer()==view._sortOptions[1].GetPointer());
    assert(view.MoveFocus(view._musicOptions[3],D::Right,nullptr).GetPointer()==view._musicOptions[0].GetPointer());
    assert(view.MoveFocus(view._musicOptions[0],D::Left,nullptr).GetPointer()==view._musicOptions[3].GetPointer());
    assert(!view.MoveFocus(view._musicOptions[0],D::Down,nullptr).GetPointer());
}
'''
        with tempfile.TemporaryDirectory() as folder:
            tmp=Path(folder);(tmp/'test.cpp').write_text(program)
            subprocess.run([os.environ.get('CXX','c++'),'-std=c++20',str(tmp/'test.cpp'),'-o',str(tmp/'test')],check=True)
            subprocess.run([str(tmp/'test')],check=True)

if __name__=='__main__':
    unittest.main()
