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
        const auto& r = recentChannels[index];
        HistoryManager::get()->add(r);
        Intent::openLive(r, [recycler]() {
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
    recyclingGrid->setDataSource(new DataSourceRecentChannels(recent));

        this->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->toggleFavorite();
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