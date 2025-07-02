

#include <iostream>
#include <borealis/core/application.hpp>

#include "activity/live_player_activity.hpp"

#include "activity/settings_activity.hpp"

#include "activity/main_activity.hpp"
#include "activity/hint_activity.hpp"

#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"

void Intent::openLive(const std::vector<tsvitch::LiveM3u8>& channelList, size_t index, std::function<void()> onClose) {
    auto activity = new LiveActivity(channelList, index, onClose);
    brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
    registerFullscreen(activity);
}

void Intent::openSettings(std::function<void()> onClose) {
    auto activity = new SettingsActivity(onClose);
    brls::Application::pushActivity(activity);
    registerFullscreen(activity);
}

void Intent::openHint() { brls::Application::pushActivity(new HintActivity()); }

void Intent::openMain() {
    auto activity = new MainActivity();
    brls::Application::pushActivity(activity);
    registerFullscreen(activity);
}

void Intent::_registerFullscreen(brls::Activity* activity) { (void)activity; }
