#pragma once
#include "FavoritePaths.h"

class FavoritesRepository
{
public:
    void Load();
    bool Toggle(const char* path);
    const FavoritePaths& GetFavorites() const { return _favorites; }
private:
    FavoritePaths _favorites;
    bool Read(const char* path, FavoritePaths& result) const;
    bool Save(const FavoritePaths& favorites) const;
};
