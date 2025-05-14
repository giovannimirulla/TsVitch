#pragma once
#include <unordered_set>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

class FavoriteManager {
public:
    explicit FavoriteManager(const std::filesystem::path& dataDir);

    void toggle(const std::string& channelId);   ///< Ajoute ou retire
    bool isFavorite(const std::string& channelId) const;

    void save() const;
    void load();

private:
    std::filesystem::path file_;
    std::unordered_set<std::string> set_;
};
