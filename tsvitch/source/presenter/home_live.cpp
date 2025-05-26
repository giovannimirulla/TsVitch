

#include <iostream>
#include "tsvitch.h"
#include "presenter/home_live.hpp"
#include "tsvitch/result/home_live_result.h"
#include "borealis/core/i18n.hpp"
#include "core/ChannelManager.hpp"

using namespace brls::literals;

void HomeLiveRequest::onLiveList(const tsvitch::LiveM3u8ListResult& result, bool firstLoad) {}

void HomeLiveRequest::onError(const std::string& error) {
    brls::Logger::error("HomeLiveRequest: Error: {}", error);
}

void HomeLiveRequest::requestLiveList() {
    CLIENT::get_file_m3u8(
        [this](const auto& result) {
            UNSET_REQUEST

            tsvitch::LiveM3u8ListResult res = result;
            this->onLiveList(res, true);
        },
        [this](const std::string &error, int code) { 
            this->onError("Failed to fetch live list: " + error);
            UNSET_REQUEST;
        }
    );
}