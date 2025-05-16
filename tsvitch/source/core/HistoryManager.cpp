#include "core/HistoryManager.hpp"
#include <fstream>
#include <filesystem>
#include <nlohmann/json.hpp>

using json = nlohmann::json;
HistoryManager::HistoryManager(const std::filesystem::path& dataDir)
    : file_{dataDir / "history.json"} {
    load();
}

void HistoryManager::add(const std::string& id) {
    if (!ring_.empty() && ring_.front() == id) return;
    ring_.push_front(id);
    if (ring_.size() > MAX_ITEMS) ring_.pop_back();
}

std::deque<std::string> HistoryManager::recent(std::size_t limit) const {
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
    }
}
