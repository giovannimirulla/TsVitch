#include "core/FavoriteManager.hpp"
#include "utils/config_helper.hpp"
#include <fstream>
#include <iostream>

using json = nlohmann::json;

FavoriteManager::FavoriteManager(const std::filesystem::path& dataDir)
    : file_{dataDir / "favorites.json"} {
    load();
}

FavoriteManager* FavoriteManager::get() {
    const std::string path = ProgramConfig::instance().getConfigDir();
    static FavoriteManager instance(path);
    return &instance;
}

void FavoriteManager::toggle(const tsvitch::LiveM3u8& channel) {
    auto it = std::find_if(set_.begin(), set_.end(),
        [&](const tsvitch::LiveM3u8& c){ return c.url == channel.url; });
    if (it != set_.end()) {
        set_.erase(it);
    } else {
        set_.push_back(channel);
    }
    save();
}


bool FavoriteManager::isFavorite(const std::string& url ) const {
    auto it = std::find_if(set_.begin(), set_.end(),
        [&](const tsvitch::LiveM3u8& c){ return  c.url == url; });
    return it != set_.end();
}

void FavoriteManager::save() const {
    json j = set_;
    std::ofstream(file_) << j.dump(2);
}

void FavoriteManager::load() {
    if (std::ifstream in{file_}; in) {
        json j; in >> j;
        set_ = j.get<decltype(set_)>();
    }
}
std::vector<tsvitch::LiveM3u8> FavoriteManager::getFavorites() const {
    return std::vector<tsvitch::LiveM3u8>(set_.begin(), set_.end());
}
