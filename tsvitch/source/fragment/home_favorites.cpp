//
// Mostra i canali preferiti dell'utente
//

#include <utility>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "view/custom_button.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"
#include "fragment/home_favorites.hpp"
#include "core/FavoriteManager.hpp"
#include "core/HistoryManager.hpp"
#include "api/tsvitch/result/home_live_result.h"  // aggiungi questa riga

using namespace brls::literals;

// DataSource per i canali preferiti
class DataSourceFavoriteChannels : public RecyclingGridDataSource {
public:
    explicit DataSourceFavoriteChannels(const std::vector<tsvitch::LiveM3u8>& favorites)
        : favoriteChannels(favorites) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        const auto& r                        = favoriteChannels[index];
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(r);
        return item;
    }

    size_t getItemCount() override { return favoriteChannels.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        HistoryManager::get()->add(favoriteChannels[index]);
        Intent::openLive(favoriteChannels, index, [recycler]() {
            auto favorites = FavoriteManager::get()->getFavorites();
            recycler->setDataSource(new DataSourceFavoriteChannels(favorites));
        });
    }

    void clearData() override { favoriteChannels.clear(); }

private:
    std::vector<tsvitch::LiveM3u8> favoriteChannels;
};

/// HomeFavorites

HomeFavorites::HomeFavorites() {
    this->inflateFromXMLRes("xml/fragment/home_favorites.xml");
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    // Carica i canali preferiti
    this->favoritesList = FavoriteManager::get()->getFavorites();
    recyclingGrid->setDataSource(new DataSourceFavoriteChannels(this->favoritesList));

    this->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->toggleFavorite();
        return true;
    });
}


void HomeFavorites::onCreate() { this->refreshFavorites(); }

void HomeFavorites::onShow() { this->refreshFavorites(); }

void HomeFavorites::refreshFavorites() {
    this->favoritesList = FavoriteManager::get()->getFavorites();
    recyclingGrid->setDataSource(new DataSourceFavoriteChannels(this->favoritesList));
}

void HomeFavorites::toggleFavorite() {
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) return;

    tsvitch::LiveM3u8 channel = item->getChannel();

    brls::Logger::debug("toggleFavorite: {}", channel.title);

    FavoriteManager::get()->toggle(channel);

    if (FavoriteManager::get()->isFavorite(channel.url)) {
        item->setFavoriteIcon(true);
    } else {
        item->setFavoriteIcon(false);
        // rimuovi l'item dalla lista
        auto it = std::remove_if(this->favoritesList.begin(), this->favoritesList.end(),
                                 [&channel](const tsvitch::LiveM3u8& c) { return c.url == channel.url; });
        size_t removedIndex = std::distance(this->favoritesList.begin(), it);
        this->favoritesList.erase(it, this->favoritesList.end());
        this->recyclingGrid->setDataSource(new DataSourceFavoriteChannels(this->favoritesList));

        if (!this->favoritesList.empty()) {
            size_t newFocus = removedIndex;
            if (newFocus >= this->favoritesList.size() && newFocus > 0)
                newFocus = this->favoritesList.size() - 1;

            // Dai focus alla griglia e poi alla cella, con un piccolo delay per sicurezza
            brls::Application::giveFocus(this->recyclingGrid);
            brls::delay(10, [this, newFocus]() {
                this->recyclingGrid->setDefaultCellFocus(newFocus);
            });
        }else {
           //focus sidebar
            brls::Application::giveFocus(this->getTabBar());
        }
    }
}

brls::View* HomeFavorites::create() { return new HomeFavorites(); }

HomeFavorites::~HomeFavorites() = default;

void HomeFavorites::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}