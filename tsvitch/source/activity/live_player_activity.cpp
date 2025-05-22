#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>

#include "activity/live_player_activity.hpp"
#include "utils/number_helper.hpp"

#include <vector>
#include <chrono>

#include "utils/shader_helper.hpp"
#include "utils/config_helper.hpp"

#include "view/video_view.hpp"

#include "view/grid_dropdown.hpp"
#include "view/qr_image.hpp"
#include "view/mpv_core.hpp"

#include "config/ad_config.h"

#include "core/FavoriteManager.hpp"

using namespace brls::literals;

LiveActivity::LiveActivity(const tsvitch::LiveM3u8& liveData, std::function<void()> onClose)
    : onCloseCallback(onClose)
{
    brls::Logger::debug("LiveActivity: create: {}", liveData.title);
    this->liveData = liveData;
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

    std::string adUrl = this->getAdUrlFromServer();
    if (!adUrl.empty()) {
        this->startAd(adUrl);
    } else {
        this->startLive();
    }

    GA("open_live", {{"title", this->liveData.title}})
}
void LiveActivity::startAd(std::string adUrl) {
    brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
    this->video->setUrl(adUrl);
    this->video->setOnEndCallback([this]() {
        this->video->setOnEndCallback(nullptr);  // Rimuovi callback per evitare loop
        this->startLive();
    });
}

void LiveActivity::startLive() {
    brls::Logger::debug("LiveActivity: start live");
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
    std::string adUrl = this->getAdUrlFromServer();
    this->video->setUrl(adUrl);
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

std::string LiveActivity::getAdUrlFromServer() {
    // Qui puoi implementare una chiamata HTTP per ottenere l'URL dell'ad
    return AD_SERVER_URL_VALUE;
}

LiveActivity::~LiveActivity() {
    brls::Logger::debug("LiveActivity: delete");
    this->video->stop();
    brls::cancelDelay(toggleDelayIter);
    brls::cancelDelay(errorDelayIter);
    if (onCloseCallback)
        onCloseCallback();
}