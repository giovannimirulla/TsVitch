

#pragma once

#include "view/auto_tab_frame.hpp"
#include "presenter/home_live.hpp"

#include <map>
#include <mutex>

typedef brls::Event<std::string> UpdateSearchEvent;

namespace brls {
class Label;
};
class RecyclingGrid;
class CustomButton;

class HomeLive : public AttachedView, public HomeLiveRequest {
public:
    HomeLive();

    void onLiveList(const tsvitch::LiveM3u8ListResult &result, bool firstLoad) override;

    ~HomeLive() override;

    void onCreate() override;

    void onError(const std::string &error) override;

    void onShow() override;

    void search();

    void cancelSearch();

    void toggleFavorite();

    void downloadVideo();

    void selectGroupIndex(size_t index);

    void filter(const std::string &key);

    void setSearchCallback(UpdateSearchEvent *event);

    static View *create();

private:
    int selectedGroupIndex = 0;
    bool isSearchActive    = false;
    bool isInitialLoadInProgress = false;
    tsvitch::LiveM3u8ListResult channelsList;
    std::map<std::string, tsvitch::LiveM3u8ListResult> groupCache;
    std::mutex groupCacheMutex;
    std::shared_ptr<std::atomic<bool>> validityFlag;
    brls::Event<>::Subscription exitEventSubscription;
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/live/recyclingGrid");
    BRLS_BIND(RecyclingGrid, upRecyclingGrid, "dynamic/up/recyclingGrid");
    BRLS_BIND(CustomButton, searchField, "home/search");
};