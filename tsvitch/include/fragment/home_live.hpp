

#pragma once

#include "view/auto_tab_frame.hpp"
#include "presenter/home_live.hpp"

#include <map>
#include <mutex>
#include <set>

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

    // Shows the initial hub with the 3 cards (Live TV / Movies / Series)
    void showContentHub();

    // Enters an Xtream content type (0 = Live TV, 1 = Movies/VOD, 2 = Series),
    // hides the hub, shows the category sidebar + grid and loads the content
    void enterContentType(int contentType);

    // Opens the episodes of a series (seasons in the sidebar, episodes in the grid)
    void openSeriesEpisodes(const tsvitch::LiveM3u8& series);

    // Updates the search/refresh button labels according to the active content type
    void updateActionLabels();

    // Clears the cache of the current view and reloads it from the server
    void refreshCurrent();

    // Shows a category's channels, asking for the parental PIN first if it is locked
    void selectGroupContent(const std::string& group);

    // Opens the PIN prompt; calls onUnlock on the correct PIN
    void promptCategoryPin(const std::string& category, std::function<void()> onUnlock);

    void filter(const std::string &key);

    void setSearchCallback(UpdateSearchEvent *event);

    static View *create();

private:
    int selectedGroupIndex = 0;
    bool isSearchActive    = false;
    bool isInitialLoadInProgress = false;
    bool isXtreamMode      = false;  // true when IPTV_MODE == Xtream
    bool inHubMode         = false;  // true when the 3-card hub is visible
    bool inSeriesEpisodes  = false;  // true when showing a series' episodes
    int  currentLoadType   = 0;      // content type being shown (0=Live, 1=Movies, 2=Series)
    std::string currentSeriesId;     // series_id of the episodes currently shown (for the cache)
    std::string currentSeriesTitle;  // title of the series currently shown

    // In-memory caches so going back does not refetch from the server
    std::map<int, tsvitch::LiveM3u8ListResult> contentCache;          // per content type
    std::map<std::string, tsvitch::LiveM3u8ListResult> episodesCache; // per series_id
    std::set<std::string> unlockedCategories;                        // categories unlocked this session
    tsvitch::LiveM3u8ListResult channelsList;
    std::map<std::string, tsvitch::LiveM3u8ListResult> groupCache;
    std::mutex groupCacheMutex;
    std::shared_ptr<std::atomic<bool>> validityFlag;
    brls::Event<>::Subscription exitEventSubscription;
    bool hasExitSubscription = false;
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/live/recyclingGrid");
    BRLS_BIND(RecyclingGrid, upRecyclingGrid, "dynamic/up/recyclingGrid");
    BRLS_BIND(CustomButton, searchField, "home/search");
    BRLS_BIND(brls::Label, searchLabel, "home/search/label");
    BRLS_BIND(CustomButton, refreshButton, "home/refresh");
    BRLS_BIND(brls::Label, refreshLabel, "home/refresh/label");
    BRLS_BIND(brls::Box, leftColumn, "xtream/left/column");
    BRLS_BIND(brls::Box, contentHub, "xtream/hub");
    BRLS_BIND(CustomButton, hubLive, "xtream/hub/live");
    BRLS_BIND(CustomButton, hubMovies, "xtream/hub/movies");
    BRLS_BIND(CustomButton, hubSeries, "xtream/hub/series");
    BRLS_BIND(CustomButton, backButton, "xtream/content/back");
    BRLS_BIND(brls::Label, backLabel, "xtream/content/back/label");
};