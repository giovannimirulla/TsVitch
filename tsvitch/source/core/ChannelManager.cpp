#include "core/ChannelManager.hpp"
#include "utils/config_helper.hpp"
#include <fstream>

using json = nlohmann::json;

ChannelManager::ChannelManager(const std::filesystem::path& dataDir)
    : file_{dataDir / "channels.json"} {}

ChannelManager* ChannelManager::get() {
    const std::string path = ProgramConfig::instance().getConfigDir();
    static ChannelManager instance(path);
    return &instance;
}

void ChannelManager::save(const tsvitch::LiveM3u8ListResult& channels) const {
    json j = channels;
    std::ofstream(file_) << j.dump(2);
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