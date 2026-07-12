

#pragma once

#include "view/auto_tab_frame.hpp"
#include "presenter/home_live.hpp"

#include <map>
#include <mutex>

typedef brls::Event<std::string> UpdateSearchEvent;

namespace brls {
class Label;
class Box;
};
class RecyclingGrid;
class CustomButton;

class HomeLive : public AttachedView, public HomeLiveRequest {
public:
    HomeLive();

    void onLiveList(tsvitch::LiveM3u8ListResult result, bool firstLoad) override;

    ~HomeLive() override;

    void onCreate() override;

    void onError(const std::string &error) override;

    void onShow() override;

    void search();

    void cancelSearch();

    void toggleFavorite();

    void downloadVideo();

    void selectGroupIndex(size_t index);

    // Mostra o hub inicial com os 3 cards (TV ao Vivo / Filmes / Séries)
    void showContentHub();

    // Entra num tipo de conteúdo Xtream (0 = Live TV, 1 = Filmes/VOD, 2 = Séries),
    // esconde o hub, mostra a barra de categorias + a grade e carrega o conteúdo
    void enterContentType(int contentType);

    void filter(const std::string &key);

    void setSearchCallback(UpdateSearchEvent *event);

    static View *create();

private:
    int selectedGroupIndex = 0;
    bool isSearchActive    = false;
    bool isInitialLoadInProgress = false;
    bool isXtreamMode      = false;  // true quando IPTV_MODE == Xtream
    bool inHubMode         = false;  // true quando o hub de 3 cards está visível
    tsvitch::LiveM3u8ListResult channelsList;
    std::map<std::string, tsvitch::LiveM3u8ListResult> groupCache;
    std::mutex groupCacheMutex;
    std::shared_ptr<std::atomic<bool>> validityFlag;
    brls::Event<>::Subscription exitEventSubscription;
    bool hasExitSubscription = false;
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/live/recyclingGrid");
    BRLS_BIND(RecyclingGrid, upRecyclingGrid, "dynamic/up/recyclingGrid");
    BRLS_BIND(CustomButton, searchField, "home/search");
    BRLS_BIND(brls::Box, leftColumn, "xtream/left/column");
    BRLS_BIND(brls::Box, contentHub, "xtream/hub");
    BRLS_BIND(CustomButton, hubLive, "xtream/hub/live");
    BRLS_BIND(CustomButton, hubMovies, "xtream/hub/movies");
    BRLS_BIND(CustomButton, hubSeries, "xtream/hub/series");
    BRLS_BIND(CustomButton, backButton, "xtream/content/back");
    BRLS_BIND(brls::Label, backLabel, "xtream/content/back/label");
};