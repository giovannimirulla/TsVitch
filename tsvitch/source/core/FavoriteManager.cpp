#include "core/FavoriteManager.hpp"
#include <fstream>

using json = nlohmann::json;

FavoriteManager::FavoriteManager(const std::filesystem::path& dataDir)
    : file_{dataDir / "favorites.json"} {
    load();
}

void FavoriteManager::toggle(const std::string& id) {
    if (!set_.erase(id))        // n’était pas présent → on l’ajoute
        set_.insert(id);
}

bool FavoriteManager::isFavorite(const std::string& id) const {
    return set_.find(id) != set_.end();
}

void FavoriteManager::save() const {
    std::ofstream(file_) << json(set_).dump(2);
}

void FavoriteManager::load() {
    if (std::ifstream in{file_}; in) {
        json j; in >> j;
        set_ = j.get<decltype(set_)>();
    }
}
