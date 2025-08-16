

#include <iostream>
#include "tsvitch.h"
#include "presenter/home_live.hpp"
#include "tsvitch/result/home_live_result.h"
#include "borealis/core/i18n.hpp"
#include "core/ChannelManager.hpp"
#include "utils/config_helper.hpp"

using namespace brls::literals;

void HomeLiveRequest::onLiveList(const tsvitch::LiveM3u8ListResult& result, bool firstLoad) {}

void HomeLiveRequest::onError(const std::string& error) {
    brls::Logger::error("HomeLiveRequest: Error: {}", error);
}

HomeLiveRequest::~HomeLiveRequest() {
    brls::Logger::debug("HomeLiveRequest: destructor");
    // Invalidate the flag to prevent callbacks from accessing this object
    if (validityFlag) {
        validityFlag->store(false);
    }
}

void HomeLiveRequest::requestLiveList() {
    // Create a flag to track if this object is still valid
    auto isValidFlag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = isValidFlag;
    
    // Use the new unified function that handles both M3U8 and Xtream modes
    brls::Logger::info("HomeLiveRequest: Requesting live channels...");
    CLIENT::get_live_channels(
        [this, isValidFlag](const auto& result) {
            // Check if this object is still valid before accessing it
            if (!isValidFlag->load()) {
                brls::Logger::debug("HomeLiveRequest::requestLiveList: Object destroyed before callback");
                return;
            }
            
            try {
                UNSET_REQUEST
                tsvitch::LiveM3u8ListResult res = result;
                brls::Logger::info("HomeLiveRequest: Successfully received {} channels", res.size());
                this->onLiveList(res, true);
            } catch (...) {
                brls::Logger::error("HomeLiveRequest::requestLiveList: Exception during callback");
            }
        },
        [this, isValidFlag](const std::string &error, int code) {
            // Check if this object is still valid before accessing it
            if (!isValidFlag->load()) {
                brls::Logger::debug("HomeLiveRequest::requestLiveList: Object destroyed before error callback");
                return;
            }
            
            try {
                brls::Logger::error("HomeLiveRequest: Failed to fetch live channels: {}", error);
                this->onError("Failed to fetch live list: " + error);
                UNSET_REQUEST;
            } catch (...) {
                brls::Logger::error("HomeLiveRequest::requestLiveList: Exception during error callback");
            }
        }
    );
}