#pragma once

#include "view/auto_tab_frame.hpp"

class RecyclingGrid;
class CustomButton;

class HomeFavorites : public AttachedView {
public:
    HomeFavorites();

    ~HomeFavorites();

    static View *create();

    void onCreate() override;

    void onShow() override;

    void onError(const std::string &error);

    void refreshRecent();

    void refreshFavorites();

    void toggleFavorite();

    void downloadVideo();

private:
    bool isSearchActive = false;
    tsvitch::LiveM3u8ListResult favoritesList;
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/favorites/recyclingGrid");
};