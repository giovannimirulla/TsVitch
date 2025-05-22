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

    void search();

    void cancelSearch();

    void filter(const std::string &key);

    void refreshRecent();

    void refreshFavorites();

private:
    tsvitch::LiveM3u8ListResult favoritesList;
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/favorites/recyclingGrid");
    BRLS_BIND(CustomButton, searchField, "home/search");
};