#pragma once
#include <algorithm>
#include <string>
#include <vector>
#include <strings.h>

// Absolute SD paths, compared case-insensitively like FAT. No ROM data is stored.
class FavoritePaths
{
public:
    static constexpr unsigned MaxFavorites = 512;
    static bool IsValid(const char* path)
    {
        if (!path || path[0] != '/') return false;
        const std::string value(path);
        for (unsigned char c : value)
            if (c < 32 || c == ':' || c == '\\') return false;
        if (value.size() < 2 || value.size() >= 256 || value.back() == '/') return false;
        for (size_t start = 1; start < value.size();)
        {
            auto end = value.find('/', start);
            auto part = value.substr(start, end - start);
            if (part.empty() || part == "." || part == "..") return false;
            if (end == std::string::npos) break;
            start = end + 1;
        }
        return true;
    }
    bool Contains(const char* path) const
    {
        return path && std::any_of(_paths.begin(), _paths.end(),
            [path](const auto& entry) { return strcasecmp(entry.c_str(), path) == 0; });
    }
    bool Add(const char* path)
    {
        if (!IsValid(path) || Contains(path) || _paths.size() >= MaxFavorites) return false;
        _paths.emplace_back(path);
        return true;
    }
    bool Toggle(const char* path)
    {
        auto found = std::find_if(_paths.begin(), _paths.end(),
            [path](const auto& entry) { return path && strcasecmp(entry.c_str(), path) == 0; });
        if (found != _paths.end()) { _paths.erase(found); return true; }
        return Add(path);
    }
    const std::vector<std::string>& GetPaths() const { return _paths; }
private:
    std::vector<std::string> _paths;
};
