//
// Mostra i canali recenti visti dall'utente
//

#include <utility>
#include <borealis/core/thread.hpp>
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"
#include "fragment/home_history.hpp"
#include "core/HistoryManager.hpp"
#include "core/FavoriteManager.hpp"
#include "core/DownloadManager.hpp"

using namespace brls::literals;

// DataSource per i canali recenti
class DataSourceRecentChannels : public RecyclingGridDataSource {
public:
    explicit DataSourceRecentChannels(const std::deque<tsvitch::LiveM3u8>& recent)
        : recentChannels(recent.begin(), recent.end()) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        const auto& r                        = recentChannels[index];
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(r);
        return item;
    }

    size_t getItemCount() override { return recentChannels.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        HistoryManager::get()->add(recentChannels[index]);
        Intent::openLive(recentChannels, index, [recycler]() {
            auto recent = HistoryManager::get()->recent(8);
            recycler->setDataSource(new DataSourceRecentChannels(recent));
        });
    }

    void clearData() override { recentChannels.clear(); }

private:
    std::vector<tsvitch::LiveM3u8> recentChannels;
};

/// HomeHistory

HomeHistory::HomeHistory() {
    this->inflateFromXMLRes("xml/fragment/home_history.xml");
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    // Carica i canali recenti dalla cronologia
    auto recent = HistoryManager::get()->recent(8);
    recyclingGrid->setDataSource(new DataSourceRecentChannels(recent));        this->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->toggleFavorite();
        return true;
    });
    
    this->registerAction("Scarica video", brls::BUTTON_RT, [this](...) {
        this->downloadVideo();
        return true;
    });
    
}

void HomeHistory::toggleFavorite(){
    //get focus item
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) return;

    //get channel
    tsvitch::LiveM3u8 channel = item->getChannel();

   
    FavoriteManager::get()->toggle(channel);

    if (FavoriteManager::get()->isFavorite(channel.url)) {
        item->setFavoriteIcon(true);
    } else {
        item->setFavoriteIcon(false);
    }
}

void HomeHistory::onCreate() { this->refreshRecent(); }

void HomeHistory::onShow() { this->refreshRecent(); }
void HomeHistory::refreshRecent() {
    auto recent = HistoryManager::get()->recent(8);
    recyclingGrid->setDataSource(new DataSourceRecentChannels(recent));
}

brls::View* HomeHistory::create() { return new HomeHistory(); }

HomeHistory::~HomeHistory() = default;

void HomeHistory::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}

void HomeHistory::downloadVideo() {
    // Ottieni l'item attualmente focalizzato
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeHistory::downloadVideo: No focused item");
        return;
    }

    // Ottieni il canale
    tsvitch::LiveM3u8 channel = item->getChannel();
    
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
        brls::Logger::info("HomeHistory: Started download {} for {}", downloadId, channel.title);
    } else {
        brls::Application::notify("Errore nell'avvio del download");
        brls::Logger::error("HomeHistory: Failed to start download for {}", channel.title);
    }
}