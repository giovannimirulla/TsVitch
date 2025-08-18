#include "core/ChannelManager.hpp"
#include "utils/config_helper.hpp"
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <chrono>

using json = nlohmann::json;

ChannelManager::ChannelManager(const std::filesystem::path& dataDir) : 
    file_{dataDir / "channels.json"}, 
    timestampFile_{dataDir / "channels_timestamp.txt"} {}

ChannelManager* ChannelManager::get() {
    const std::string path = ProgramConfig::instance().getConfigDir();
    static ChannelManager instance(path);
    return &instance;
}

void ChannelManager::save(const tsvitch::LiveM3u8ListResult& channels) const {
    json j = channels;
    std::ofstream(file_) << j.dump(2);
}

void ChannelManager::saveWithTimestamp(const tsvitch::LiveM3u8ListResult& channels) const {
    brls::Logger::info("ChannelManager: Starting saveWithTimestamp for {} channels", channels.size());
    brls::Logger::info("ChannelManager: Cache file path: {}", file_.string());
    brls::Logger::info("ChannelManager: Timestamp file path: {}", timestampFile_.string());
    
    // Crea le directory se non esistono
    std::error_code ec;
    std::filesystem::create_directories(file_.parent_path(), ec);
    if (ec) {
        brls::Logger::error("ChannelManager: Failed to create directory: {}", ec.message());
        return;
    }
    
    brls::Logger::info("ChannelManager: Directory created successfully, saving channels...");
    
    // Salva i canali
    save(channels);
    
    brls::Logger::info("ChannelManager: Channels saved, saving timestamp...");
    
    // Salva il timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    brls::Logger::info("ChannelManager: Saving timestamp: {}", timestamp);
    
    std::ofstream timestampStream(timestampFile_);
    if (!timestampStream) {
        brls::Logger::error("ChannelManager: Failed to open timestamp file for writing");
        return;
    }
    
    timestampStream << timestamp;
    timestampStream.flush();
    timestampStream.close();
    
    brls::Logger::info("ChannelManager: Cache and timestamp saved successfully");
    brls::Logger::info("ChannelManager: Verifying files exist...");
    
    if (std::filesystem::exists(file_)) {
        brls::Logger::info("ChannelManager: Cache file exists: {}", file_.string());
    } else {
        brls::Logger::error("ChannelManager: Cache file does not exist after save!");
    }
    
    if (std::filesystem::exists(timestampFile_)) {
        brls::Logger::info("ChannelManager: Timestamp file exists: {}", timestampFile_.string());
    } else {
        brls::Logger::error("ChannelManager: Timestamp file does not exist after save!");
    }
}

bool ChannelManager::isCacheValid(int maxAgeMinutes) const {
    brls::Logger::debug("ChannelManager: Checking cache validity...");
    brls::Logger::debug("ChannelManager: Cache file path: {}", file_.string());
    brls::Logger::debug("ChannelManager: Timestamp file path: {}", timestampFile_.string());
    
    if (!std::filesystem::exists(file_)) {
        brls::Logger::debug("ChannelManager: Cache file does not exist");
        return false;
    }
    
    if (!std::filesystem::exists(timestampFile_)) {
        brls::Logger::debug("ChannelManager: Timestamp file does not exist");
        return false;
    }
    
    std::ifstream timestampStream(timestampFile_);
    if (!timestampStream) {
        brls::Logger::debug("ChannelManager: Failed to open timestamp file");
        return false;
    }
    
    long long savedTimestamp;
    timestampStream >> savedTimestamp;
    brls::Logger::debug("ChannelManager: Saved timestamp: {}", savedTimestamp);
    
    auto now = std::chrono::system_clock::now();
    auto currentTimestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    brls::Logger::debug("ChannelManager: Current timestamp: {}", currentTimestamp);
    
    auto ageInSeconds = currentTimestamp - savedTimestamp;
    auto maxAgeInSeconds = maxAgeMinutes * 60;
    
    brls::Logger::debug("ChannelManager: Cache age: {} seconds (max allowed: {} seconds)", ageInSeconds, maxAgeInSeconds);
    
    bool isValid = ageInSeconds <= maxAgeInSeconds;
    brls::Logger::debug("ChannelManager: Cache is {}", isValid ? "VALID" : "INVALID");
    
    return isValid;
}

tsvitch::LiveM3u8ListResult ChannelManager::loadIfValid(int maxAgeMinutes) const {
    brls::Logger::debug("ChannelManager: Attempting to load valid cache (max age: {} minutes)", maxAgeMinutes);
    
    if (isCacheValid(maxAgeMinutes)) {
        brls::Logger::debug("ChannelManager: Cache is valid, loading channels");
        auto result = load();
        brls::Logger::debug("ChannelManager: Loaded {} channels from cache", result.size());
        return result;
    }
    
    brls::Logger::debug("ChannelManager: Cache is invalid or empty, returning empty result");
    return {};
}

void ChannelManager::remove() const { 
    std::remove(file_.string().c_str()); 
    std::remove(timestampFile_.string().c_str());
}

tsvitch::LiveM3u8ListResult ChannelManager::load() const {
    tsvitch::LiveM3u8ListResult channels;
    if (std::ifstream in{file_}; in) {
        json j;
        in >> j;
        channels = j.get<tsvitch::LiveM3u8ListResult>();
    }
    return channels;
}