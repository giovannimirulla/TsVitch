

#pragma once
#include <string>

class Intent {
public:
    static void openLive(const std::string& url, const std::string& title = "", const std::string& groupTitle = "");

    static void openPgcFilter(const std::string& filter);

    static void openSetting();

    static void openInbox();

    static void openHint();

    static void openMain();

    static void openGallery(const std::vector<std::string>& data);

    static void openDLNA();

    static void openSearch(const std::string& key);

    static void openActivity(const std::string& id);

    static void _registerFullscreen(brls::Activity* activity);
};

#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
#define ALLOW_FULLSCREEN
#endif

#ifdef ALLOW_FULLSCREEN
#define registerFullscreen(activity) Intent::_registerFullscreen(activity)
#else
#define registerFullscreen(activity) (void)activity
#endif