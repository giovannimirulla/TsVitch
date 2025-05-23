#pragma once

#include "view/auto_tab_frame.hpp"

class RecyclingGrid;

class HomeHistory : public AttachedView {
public:
    HomeHistory();

    // void onRecommendVideoList(const bilibili::RecommendVideoListResultWrapper &result) override;

    ~HomeHistory();

    static View *create();

    void onCreate() override;

    void onShow() override;

    void onError(const std::string &error);

    void refreshRecent();

    void toggleFavorite();

private:
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/history/recyclingGrid");
};