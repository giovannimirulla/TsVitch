//
// Mostra i canali preferiti dell'utente
//

#include <utility>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>
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
        item->setCard(r.logo, r.title, r.groupTitle, r.url, r.chno);
        return item;
    }

    size_t getItemCount() override { return favoriteChannels.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        const auto& r = favoriteChannels[index];
        HistoryManager::get()->add(r);
        Intent::openLive(r, [recycler]() {
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

    this->searchField->registerClickAction([this](brls::View* view) -> bool {
        this->search();
        return true;
    });
    this->searchField->addGestureRecognizer(new brls::TapGestureRecognizer(this->searchField));

    if (this->favoritesList.empty()) {
        this->searchField->setVisibility(brls::Visibility::GONE);
    } else {
        this->registerAction("hints/search"_i18n, brls::BUTTON_Y, [this](...) {
            this->search();
            return true;
        });
    }
}

void HomeFavorites::search() {
    brls::Application::getImeManager()->openForText([this](const std::string& text) { this->filter(text); },
                                                    "tsvitch/home/common/search"_i18n, "", 32, "", 0);
    this->registerAction("hints/cancel"_i18n, brls::BUTTON_B, [this](...) {
        this->cancelSearch();
        return true;
    });
}

void HomeFavorites::cancelSearch() {
    this->recyclingGrid->setDataSource(new DataSourceFavoriteChannels(this->favoritesList));
    this->unregisterAction(brls::BUTTON_B);
}

void HomeFavorites::filter(const std::string& key) {
    if (key.empty()) return;

    brls::Threading::sync([this, key]() {
        auto* datasource = dynamic_cast<DataSourceFavoriteChannels*>(recyclingGrid->getDataSource());
        if (datasource) {
            tsvitch::LiveM3u8ListResult filtered;
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            for (const auto& item : this->favoritesList) {
                std::string lowerTitle = item.title;
                std::transform(lowerTitle.begin(), lowerTitle.end(), lowerTitle.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                std::string lowerGroupTitle = item.groupTitle;
                std::transform(lowerGroupTitle.begin(), lowerGroupTitle.end(), lowerGroupTitle.begin(),
                               [](unsigned char c) { return std::tolower(c); });
                if (lowerTitle.find(lowerKey) != std::string::npos ||
                    lowerGroupTitle.find(lowerKey) != std::string::npos)
                    filtered.push_back(item);
            }
            if (filtered.empty()) {
                recyclingGrid->setEmpty();
            } else {
                recyclingGrid->setDataSource(new DataSourceFavoriteChannels(filtered));
            }
        }
    });
}

void HomeFavorites::onCreate() { this->refreshFavorites(); }

void HomeFavorites::onShow() { this->refreshFavorites(); }

void HomeFavorites::refreshFavorites() {
    this->favoritesList = FavoriteManager::get()->getFavorites();
    recyclingGrid->setDataSource(new DataSourceFavoriteChannels(this->favoritesList));
    if (this->favoritesList.empty()) {
        this->searchField->setVisibility(brls::Visibility::GONE);
        this->unregisterAction(brls::BUTTON_Y);
    } else {
        this->searchField->setVisibility(brls::Visibility::VISIBLE);
        this->registerAction("hints/search"_i18n, brls::BUTTON_Y, [this](...) {
            this->search();
            return true;
        });
    }
}

brls::View* HomeFavorites::create() { return new HomeFavorites(); }

HomeFavorites::~HomeFavorites() = default;

void HomeFavorites::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}