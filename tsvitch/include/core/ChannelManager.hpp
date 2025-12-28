#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include <chrono>
#include "api/tsvitch/result/home_live_result.h"

class ChannelManager {
public:
    explicit ChannelManager(const std::filesystem::path& dataDir);

    void save(const tsvitch::LiveM3u8ListResult& channels) const;
    tsvitch::LiveM3u8ListResult load() const;
    
    // Binary cache functions (faster than JSON)
    void saveBinary(const tsvitch::LiveM3u8ListResult& channels) const;
    tsvitch::LiveM3u8ListResult loadBinary() const;
    
    // Nuove funzioni per cache intelligente
    bool isCacheValid(int maxAgeMinutes = 43200) const; // Cache valida per 1 mese di default (43200 minuti)
    void saveWithTimestamp(const tsvitch::LiveM3u8ListResult& channels) const;
    tsvitch::LiveM3u8ListResult loadIfValid(int maxAgeMinutes = 43200) const;

    static ChannelManager* get();

    void remove() const;

private:
    std::filesystem::path file_;
    std::filesystem::path binaryFile_;
    std::filesystem::path timestampFile_;
    
    // Helper functions for binary serialization
    void writeString(std::ofstream& out, const std::string& str) const;
    std::string readString(std::ifstream& in) const;
};