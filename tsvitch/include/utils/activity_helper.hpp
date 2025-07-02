#pragma once
#include <string>
#include <borealis/core/event.hpp> // aggiungi questa riga
#include "api/tsvitch/result/home_live_result.h" // aggiungi questa riga

class Intent {
public:

    static void openLive(const std::vector<tsvitch::LiveM3u8>& channelList, size_t index, std::function<void()> onClose);

    static void openPgcFilter(const std::string& filter);

    static void openSettings(std::function<void()> onClose = nullptr);

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

// Evento globale per notificare il cambio M3U8
inline brls::Event<> OnM3U8UrlChanged;

// Evento globale per notificare il cambio Proxy
inline brls::Event<> OnProxyUrlChanged;