

#pragma once

#include "view/auto_tab_frame.hpp"
#include "presenter/home_live.hpp"

namespace brls {
class Label;
};
class RecyclingGrid;


class HomeLive : public AttachedView, public HomeLiveRequest {
public:
    HomeLive();

    void onLiveList(const tsvitch::LiveM3u8ListResult &result) override;

    ~HomeLive() override;

    void onCreate() override;

    void onError(const std::string &error) override;

    static View *create();

private:
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/live/recyclingGrid");
        BRLS_BIND(RecyclingGrid, upRecyclingGrid, "dynamic/up/recyclingGrid");
};