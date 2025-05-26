#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>

#include "activity/live_player_activity.hpp"
#include "utils/number_helper.hpp"

#include <vector>
#include <chrono>

#include "tsvitch.h"

#include "utils/shader_helper.hpp"
#include "utils/config_helper.hpp"

#include "view/video_view.hpp"

#include "view/grid_dropdown.hpp"
#include "view/qr_image.hpp"
#include "view/mpv_core.hpp"

#include "config/server_config.h"

#include "core/FavoriteManager.hpp"

using namespace brls::literals;

LiveActivity::LiveActivity(const std::vector<tsvitch::LiveM3u8>& channels, size_t startIndex,
                           std::function<void()> onClose)
    : onCloseCallback(onClose), channelList(channels), currentChannelIndex(startIndex) {
    this->liveData = channelList[currentChannelIndex];
    brls::Logger::debug("LiveActivity: create: {}", liveData.title);
    ShaderHelper::instance().clearShader(false);
}

void LiveActivity::onContentAvailable() {
    brls::Logger::debug("LiveActivity: onContentAvailable");

    MPVCore::instance().setAspect(
        ProgramConfig::instance().getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"}));

    this->video->registerAction("", brls::BUTTON_B, [this](...) {
        if (this->video->isOSDLock()) {
            this->video->toggleOSD();
        } else {
            if (this->video->getTvControlMode() && this->video->isOSDShown()) {
                this->video->toggleOSD();
                return true;
            }
            brls::Logger::debug("exit live");
            brls::Application::popActivity();
        }
        return true;
    });

    this->video->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->video->toggleFavorite();
        return true;
    });

    //Button R go to next channel
    this->video->registerAction("hints/next_channel"_i18n, brls::BUTTON_RB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex + 1 < channelList.size()) {
                    this->video->stop();

                    currentChannelIndex++;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string& adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    //exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });
    //Button L go to previous channel
    this->video->registerAction("hints/previous_channel"_i18n, brls::BUTTON_LB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex > 0) {
                    this->video->stop();

                    currentChannelIndex--;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string& adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    //exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });

    this->video->hideSubtitleSetting();
    this->video->hideVideoRelatedSetting();
    this->video->hideBottomLineSetting();
    this->video->hideHighlightLineSetting();
    this->video->disableCloseOnEndOfFile();
    this->video->setFullscreenIcon(true);
    this->video->setTitle(liveData.title);
    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
    this->video->setStatusLabelLeft("");
    this->video->setFavoriteCallback([this](bool state) { FavoriteManager::get()->toggle(this->liveData); });

    this->getAdUrlFromServer([&](const std::string& adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->startAd(adUrl);
        } else {
            this->startLive();
        }
    });

    GA("open_live", {
        {"title", this->liveData.title},
        {"url", this->liveData.url},
        {"group", this->liveData.groupTitle},
        {"index", std::to_string(currentChannelIndex)},
    });
}
void LiveActivity::startAd(std::string adUrl) {
    brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
    this->isAd = true;
    this->video->setVideoMode();
    this->video->showVideoProgressSlider();
    this->video->setUrl(adUrl);

    this->video->setOnEndCallback([this]() { this->startLive(); });
}

void LiveActivity::startLive() {
    this->isAd = false;
    this->video->setLiveMode();
    this->video->hideVideoProgressSlider();

    this->video->setCustomToggleAction([this]() {
        if (MPVCore::instance().isStopped()) {
            this->onLiveData(this->liveData.url);
        } else if (MPVCore::instance().isPaused()) {
            MPVCore::instance().resume();
        } else {
            this->video->showOSD(false);
            MPVCore::instance().pause();
            brls::cancelDelay(toggleDelayIter);
            ASYNC_RETAIN
            toggleDelayIter = brls::delay(5000, [ASYNC_TOKEN]() {
                ASYNC_RELEASE
                if (MPVCore::instance().isPaused()) {
                    MPVCore::instance().stop();
                }
            });
        }
    });
    this->video->setUrl(liveData.url);
}

void LiveActivity::onLiveData(std::string url) {
    brls::Logger::debug("Live stream url: {}", url);
    this->getAdUrlFromServer([&](const std::string& adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->video->setUrl(adUrl);
        } else {
            this->startLive();
        }
    });
    return;
}

void LiveActivity::onError(const std::string& error) {
    brls::Logger::error("ERROR request live data: {}", error);
    this->video->showOSD(false);
    this->retryRequestData();
}

void LiveActivity::retryRequestData() {
    brls::cancelDelay(errorDelayIter);
    errorDelayIter = brls::delay(2000, [this]() {
        if (!MPVCore::instance().isPlaying()) this->requestData(liveData.url);
    });
}

void LiveActivity::getAdUrlFromServer(std::function<void(const std::string&)> callback) {
    CLIENT::get_ad(
        [callback](const std::string& adUrl, int statusCode) {
            if (statusCode == 200 && !adUrl.empty()) {
                brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                if (callback) callback(adUrl);
            } else {
                brls::Logger::error("LiveActivity: Failed to get ad URL, status code: {}", statusCode);
                if (callback) callback("");
            }
        },
        [callback](const std::string& error, int statusCode) {
            brls::Logger::error("LiveActivity: Error getting ad URL: {}, status code: {}", error, statusCode);
            if (callback) callback("");
        });
}

LiveActivity::~LiveActivity() {
    brls::Logger::debug("LiveActivity: delete");
    if (this->video) {
        this->video->setOnEndCallback(nullptr);  // Annulla la callback per evitare crash
        this->video->stop();
    }
    brls::cancelDelay(toggleDelayIter);
    brls::cancelDelay(errorDelayIter);
    if (onCloseCallback) onCloseCallback();
}