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
#include "core/DownloadManager.hpp"
#include "api/tsvitch/result/home_live_result.h"

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

    this->registerAction("Scarica video", brls::BUTTON_RT, [this](...) {
        this->downloadVideo();
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

void HomeFavorites::downloadVideo() {
    // Ottieni l'item attualmente focalizzato
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeFavorites::downloadVideo: No focused item");
        return;
    }

    // Ottieni il canale
    tsvitch::LiveM3u8 channel = item->getChannel();
    
    // Controlla se è una live stream in corso
    std::string url = channel.url;
    std::string title = channel.title;
    
    // Converte tutto in minuscolo per confronto case-insensitive
    std::string urlLower = url;
    std::string titleLower = title;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
    
    // Determina se è una live stream
    bool isLiveStream = false;
    
    // Indicatori di live stream negli URL e titoli
    if (urlLower.find("live") != std::string::npos || 
        urlLower.find("stream") != std::string::npos ||
        urlLower.find(".m3u8") != std::string::npos ||
        urlLower.find(".ts") != std::string::npos ||
        titleLower.find("live") != std::string::npos ||
        titleLower.find("diretta") != std::string::npos) {
        isLiveStream = true;
    }
    
    // Se è una live stream, mostra errore e blocca il download
    if (isLiveStream) {
        brls::Logger::warning("HomeFavorites: Cannot download live streams");
        brls::Dialog* dialog = new brls::Dialog("Impossibile scaricare una diretta in corso.\nIl download è disponibile solo per i contenuti on-demand.");
        dialog->addButton("OK", []() {});
        dialog->open();
        return;
    }
    
    // Avvia il download
    std::string downloadId = DownloadManager::instance().startDownload(
        channel.title, 
        channel.url, 
        channel.logo,  // URL dell'immagine
        [](const std::string& id, float progress, size_t downloaded, size_t total) {
            // Callback di progresso
            brls::Logger::debug("Download {}: {:.1f}% ({}/{} bytes)", id, progress, downloaded, total);
        },
        [](const std::string& id, const std::string& filePath) {
            // Callback di completamento
            brls::Logger::info("Download {} completed: {}", id, filePath);
            brls::sync([]() {
                brls::Application::notify("Download completato!");
            });
        },
        [](const std::string& id, const std::string& error) {
            // Callback di errore
            brls::Logger::error("Download {} failed: {}", id, error);
            brls::sync([error]() {
                brls::Application::notify("Errore download: " + error);
            });
        }
    );
    
    if (!downloadId.empty()) {
        brls::Application::notify("Download avviato: " + channel.title);
        brls::Logger::info("HomeFavorites: Started download {} for {}", downloadId, channel.title);
    } else {
        brls::Application::notify("Errore nell'avvio del download");
        brls::Logger::error("HomeFavorites: Failed to start download for {}", channel.title);
    }
}

brls::View* HomeFavorites::create() { return new HomeFavorites(); }

HomeFavorites::~HomeFavorites() = default;

void HomeFavorites::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}