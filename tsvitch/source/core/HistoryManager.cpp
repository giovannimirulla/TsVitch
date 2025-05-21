#include "core/HistoryManager.hpp"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>


using json = nlohmann::json;

HistoryManager::HistoryManager(const std::filesystem::path& dataDir)
    : file_{dataDir / "history.json"} {
    load();
}

HistoryManager* HistoryManager::get() {
    static HistoryManager instance(std::filesystem::current_path()); // o il path dati corretto
    return &instance;
}

void HistoryManager::add(const tsvitch::LiveM3u8& channel) {
    // Rimuovi eventuali duplicati (basato su id)
    ring_.erase(std::remove_if(ring_.begin(), ring_.end(),
        [&](const tsvitch::LiveM3u8& c){ return c.id == channel.id; }), ring_.end());
    // Inserisci in testa
    ring_.push_front(channel);
    // Mantieni solo MAX_ITEMS elementi
    while (ring_.size() > MAX_ITEMS)
        ring_.pop_back();
    save();
}

std::deque<tsvitch::LiveM3u8> HistoryManager::recent(std::size_t limit) const {
    return {ring_.begin(), ring_.begin() + std::min(limit, ring_.size())};
}

void HistoryManager::save() const {
    json j = ring_;
    std::ofstream(file_) << j.dump(2);
}

void HistoryManager::load() {
    if (std::ifstream in{file_}; in) {
        json j; in >> j;
        ring_ = j.get<decltype(ring_)>();
        // Se il file contiene più di MAX_ITEMS, tronca la coda
        while (ring_.size() > MAX_ITEMS)
            ring_.pop_back();
    }
}
