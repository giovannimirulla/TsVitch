#pragma once
#include <vector>
#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "api/tsvitch/result/home_live_result.h"

class ChannelManager {
public:
    explicit ChannelManager(const std::filesystem::path& dataDir);

    void save(const tsvitch::LiveM3u8ListResult& channels) const;
    tsvitch::LiveM3u8ListResult load() const;

    static ChannelManager* get();

private:
    std::filesystem::path file_;
};