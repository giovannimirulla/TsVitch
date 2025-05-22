#pragma once
#include <deque>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include "api/tsvitch/result/home_live_result.h"  // aggiungi questo include per LiveM3u8

class FavoriteManager {
public:
    explicit FavoriteManager(const std::filesystem::path& dataDir);

    void toggle(const tsvitch::LiveM3u8& channel);
    bool isFavorite(const std::string& url ) const;

    std::vector<tsvitch::LiveM3u8> getFavorites() const;

    void save() const;
    void load();

    //get istantance
    static FavoriteManager* get();

private:
    std::filesystem::path file_;
    std::deque<tsvitch::LiveM3u8> set_;
};
