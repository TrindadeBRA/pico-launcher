"""Run the production selection/navigation methods with host-side I/O/view doubles.

Requires Python 3 and a C++17 compiler. This does not emulate DS rendering or FAT I/O.
"""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / 'arm9/source'


def without_includes(text):
    return '\n'.join(line for line in text.splitlines()
                     if not line.startswith(('#include', '#pragma once')))


def method(text, signature):
    start = text.index(signature)
    brace = text.index('{', start)
    depth = 1
    end = brace + 1
    while depth:
        depth += (text[end] == '{') - (text[end] == '}')
        end += 1
    return text[start:end]


class ThemeSelectorTest(unittest.TestCase):
    def test_production_selection_and_focus(self):
        prelude = r'''
#include <algorithm>
#include <array>
#include <cassert>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <strings.h>
using u32 = unsigned;
template<class T, int N> struct String {
    std::string value;
    String(const char* s = "") : value(s) {}
    operator const char*() const { return value.c_str(); }
    const char* GetString() const { return value.c_str(); }
};
enum class RomBrowserLayout { A, B };
enum class RomBrowserSortMode { A, B };
struct RomBrowserDisplaySettings {
    RomBrowserLayout layout{};
    RomBrowserSortMode sortMode{};
};
struct IRomBrowserController {
    RomBrowserDisplaySettings settings;
    std::string theme = "material";
    int changes = 0;
    bool closed = false;
    const auto& GetRomBrowserDisplaySettings() { return settings; }
    void SetRomBrowserDisplaySettings(const RomBrowserDisplaySettings& s) { settings = s; }
    const char* GetThemeName() { return theme.c_str(); }
    void SetThemeName(const char* s) { theme = s; ++changes; }
    void HideDisplaySettings() { closed = true; }
};
constexpr int FR_OK = 0, AM_DIR = 16, AM_HID = 2, AM_SYS = 4;
struct FILINFO { char fname[256]; int fattrib; };
struct Entry { std::string name; int flags = AM_DIR; bool valid = true; };
static std::vector<Entry> entries;
static bool directoryAvailable = true;
struct Directory {
    unsigned index = 0;
    int Open(const char*) { return directoryAvailable ? FR_OK : 1; }
    int Read(FILINFO* info) {
        if (index == entries.size()) { info->fname[0] = 0; return FR_OK; }
        const auto& entry = entries[index++];
        strcpy(info->fname, entry.name.c_str()); info->fattrib = entry.flags;
        return FR_OK;
    }
};
struct ThemeInfoFactory {
    std::unique_ptr<int> CreateFromThemeFolder(const char* name) {
        for (const auto& entry : entries)
            if (entry.name == name && entry.valid) return std::make_unique<int>(1);
        return nullptr;
    }
};
'''
        view_prelude = r'''
struct View {};
struct IconButton2DView : View {};
template<class T> struct SharedPtr {
    std::shared_ptr<T> pointer;
    SharedPtr() : pointer(std::make_shared<T>()) {}
    SharedPtr(std::nullptr_t) : pointer(nullptr) {}
    template<class U> SharedPtr(const SharedPtr<U>& other) : pointer(other.pointer) {}
    T* GetPointer() const { return pointer.get(); }
};
enum class FocusMoveDirection { Up, Down, Left, Right };
struct DisplaySettingsBottomSheetView {
    std::array<SharedPtr<IconButton2DView>, 4> _layoutOptions;
    std::array<SharedPtr<IconButton2DView>, 2> _sortOptions, _themeOptions;
    SharedPtr<View> MoveFocus(const SharedPtr<View>&, FocusMoveDirection, View*);
};
'''
        assertions = r'''
int main() {
    IRomBrowserController controller;
    DisplaySettingsViewModel model(&controller);
    entries = {{"raspberry"}, {"PiCardboard Dark"}, {"material"},
        {".hidden"}, {"hidden", AM_DIR | AM_HID}, {"system", AM_DIR | AM_SYS},
        {"file", 0}, {"invalid", AM_DIR, false}, {std::string(65, 'x')}};
    model.RefreshThemes();
    assert(model.HasThemes());
    model.CycleTheme(1); assert(controller.theme == "PiCardboard Dark");
    model.CycleTheme(1); assert(controller.theme == "raspberry");
    model.CycleTheme(1); assert(controller.theme == "material");
    model.CycleTheme(-1); assert(controller.theme == "raspberry");
    controller.theme = "MATERIAL";
    model.CycleTheme(-1); assert(controller.theme == "raspberry");
    controller.theme = "missing";
    model.CycleTheme(1); assert(controller.theme == "material");
    controller.theme = "missing";
    model.CycleTheme(-1); assert(controller.theme == "raspberry");
    entries = {{"single"}};
    model.RefreshThemes(); model.CycleTheme(1); model.CycleTheme(-1);
    assert(controller.theme == "single");
    entries.clear(); model.RefreshThemes();
    int changes = controller.changes;
    model.CycleTheme(1); assert(!model.HasThemes());
    assert(controller.changes == changes);
    directoryAvailable = false; model.RefreshThemes(); model.CycleTheme(-1);
    assert(controller.changes == changes);
    model.Close(); assert(controller.closed);
    DisplaySettingsBottomSheetView view;
    using D = FocusMoveDirection;
    assert(view.MoveFocus(view._layoutOptions[3], D::Down, nullptr).GetPointer()
        == view._sortOptions[1].GetPointer());
    assert(view.MoveFocus(view._sortOptions[1], D::Down, nullptr).GetPointer()
        == view._themeOptions[1].GetPointer());
    assert(view.MoveFocus(view._themeOptions[0], D::Up, nullptr).GetPointer()
        == view._sortOptions[0].GetPointer());
    assert(view.MoveFocus(view._themeOptions[1], D::Right, nullptr).GetPointer()
        == view._themeOptions[0].GetPointer());
    assert(view.MoveFocus(view._themeOptions[0], D::Left, nullptr).GetPointer()
        == view._themeOptions[1].GetPointer());
    assert(view.MoveFocus(view._themeOptions[1], D::Down, nullptr).GetPointer()
        == view._layoutOptions[1].GetPointer());
}
'''
        model_header = (SRC / 'romBrowser/viewModels/DisplaySettingsViewModel.h').read_text()
        model_source = (SRC / 'romBrowser/viewModels/DisplaySettingsViewModel.cpp').read_text()
        view_source = (SRC / 'romBrowser/views/DisplaySettingsBottomSheetView.cpp').read_text()
        navigation = method(view_source, 'SharedPtr<View> DisplaySettingsBottomSheetView::MoveFocus')
        program = '\n'.join([prelude, without_includes(model_header),
                             without_includes(model_source), view_prelude, navigation, assertions])
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'test.cpp'
            binary = Path(directory) / 'test'
            source.write_text(program)
            subprocess.run([os.environ.get('CXX', 'c++'), '-std=c++17',
                            str(source), '-o', str(binary)], check=True)
            subprocess.run([str(binary)], check=True)


if __name__ == '__main__':
    unittest.main()
