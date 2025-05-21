#pragma once
#include <deque>
#include <filesystem>
#include <string>
#include <nlohmann/json.hpp>
#include "api/tsvitch/result/home_live_result.h" // aggiungi questo include per LiveM3u8

class HistoryManager {
public:
    explicit HistoryManager(const std::filesystem::path& dataDir);

    // Salva l'intero oggetto canale
    void add(const tsvitch::LiveM3u8& channel);

    // Restituisce gli ultimi X canali completi (default: 20)
    std::deque<tsvitch::LiveM3u8> recent(std::size_t limit = 20) const;

    void save() const;
    void load();

    //get istantance
     static HistoryManager* get();

private:
    std::filesystem::path file_;
    std::deque<tsvitch::LiveM3u8> ring_; // cambia il tipo da string a LiveM3u8
    static constexpr std::size_t MAX_ITEMS = 10;
};