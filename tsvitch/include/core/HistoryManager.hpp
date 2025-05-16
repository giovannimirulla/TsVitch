#pragma once
#include <deque>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>

class HistoryManager {
public:
    explicit HistoryManager(const std::filesystem::path& dataDir);

    /// Enregistre l’ID d’une chaîne lue
    void add(const std::string& channelId);

    /// Renvoie les X dernières chaînes (défaut : 20)
    std::deque<std::string> recent(std::size_t limit = 20) const;

    /// Sérialise vers disque (appelé à la fermeture de l’appli)
    void save() const;
    void load();               // appelé au démarrage

private:
    std::filesystem::path file_;
    std::deque<std::string> ring_;
    static constexpr std::size_t MAX_ITEMS = 100;
};
