#include <utility>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/tab_frame.hpp>

#include "fragment/home_live.hpp"
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "view/grid_dropdown.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"
#include "view/custom_button.hpp"

#include "core/HistoryManager.hpp"
#include "core/FavoriteManager.hpp"
#include "core/ChannelManager.hpp"
#include "core/DownloadManager.hpp"
#include "core/DownloadProgressManager.hpp"

#include "utils/config_helper.hpp"

using namespace brls::literals;

class DynamicGroupChannels : public RecyclingGridItem {
public:
    explicit DynamicGroupChannels(const std::string& xml) {
        this->inflateFromXMLRes(xml);
        auto theme    = brls::Application::getTheme();
        selectedColor = theme.getColor("color/tsvitch");
        fontColor     = theme.getColor("brls/text");
    }

    void setTitle(const std::string& title) { this->labelTitle->setText(title); }

    void setSelected(bool selected) { this->labelTitle->setTextColor(selected ? selectedColor : fontColor); }

    void prepareForReuse() override {
        this->labelTitle->setText("");
        this->labelTitle->setTextColor(fontColor);
    }

    void cacheForReuse() override {}

    static RecyclingGridItem* create(const std::string& xml = "xml/views/group_channel_dynamic.xml") {
        return new DynamicGroupChannels(xml);
    }

private:
    BRLS_BIND(brls::Label, labelTitle, "title");
    NVGcolor selectedColor{};
    NVGcolor fontColor{};
};

class DataSourceUpList : public RecyclingGridDataSource {
public:
    using OnGroupSelected = std::function<void(const std::string&)>;
    explicit DataSourceUpList(std::vector<std::string> result, OnGroupSelected cb = nullptr)
        : list(std::move(result)), onGroupSelected(cb) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        DynamicGroupChannels* item = (DynamicGroupChannels*)recycler->dequeueReusableCell("Cell");
        item->setTitle(this->list[index]);
        item->setSelected(index == selectedIndex);  // Imposta sempre la selezione!
        return item;
    }

    size_t getItemCount() override { return list.size(); }

    void setSelectedIndex(RecyclingGrid* recycler, size_t index) {
        brls::Logger::debug("setSelectedIndex: {}", index);
        if (index >= list.size()) return;
        selectedIndex = index;
        auto* item    = dynamic_cast<DynamicGroupChannels*>(recycler->getGridItemByIndex(index));
        if (!item) return;
        item->setSelected(true);

        // Salva l'indice selezionato
        ProgramConfig::instance().setSettingItem(SettingItem::GROUP_SELECTED_INDEX, static_cast<int>(index));

        if (onGroupSelected) onGroupSelected(list[index]);
    }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        brls::Logger::debug("onItemSelected: {}", index);
        std::vector<RecyclingGridItem*>& items = recycler->getGridItems();
        for (auto& i : items) {
            auto* cell = dynamic_cast<DynamicGroupChannels*>(i);
            if (cell) cell->setSelected(false);
        }

        selectedIndex = index;

        auto* item = dynamic_cast<DynamicGroupChannels*>(recycler->getGridItemByIndex(index));
        if (!item) return;
        item->setSelected(true);

        // Salva l'indice selezionato
        ProgramConfig::instance().setSettingItem(SettingItem::GROUP_SELECTED_INDEX, static_cast<int>(index));

        if (onGroupSelected) onGroupSelected(list[index]);
    }

    void appendData(const std::vector<std::string>& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
    }

    void clearData() override { this->list.clear(); }

    const std::string& getGroupNameByIndex(size_t index) const {
        static std::string empty;
        if (index < list.size()) return list[index];
        return empty;
    }

private:
    std::vector<std::string> list;
    size_t selectedIndex = -1;
    OnGroupSelected onGroupSelected;
};

const std::string GridMainAreaCellContentXML = R"xml(
<brls:Box
        width="auto"
        height="@style/brls/sidebar/item_height"
        focusable="true"
        paddingTop="12.5"
        paddingBottom="12.5"
        alignItems="center">

    <brls:Image
        id="area/avatar"
        scalingType="fill"
        cornerRadius="4"
        marginLeft="10"
        marginRight="10"
        width="40"
        height="40"/>

    <brls:Label
            id="area/title"
            width="auto"
            height="auto"
            grow="1"
            fontSize="22" />

</brls:Box>
)xml";

class GridMainAreaCell : public RecyclingGridItem {
public:
    GridMainAreaCell() { this->inflateFromXMLString(GridMainAreaCellContentXML); }

    void setData(const std::string& name, const std::string& pic) {
        this->title->setText(name);
        this->title->setTextColor(fontColor);

        if (pic.empty()) {
            this->image->setImageFromRes("pictures/22_open.png");
        } else {
            ImageHelper::with(image)->load(pic + ImageHelper::face_ext);
        }
    }

    void setSelected(bool value) { this->title->setTextColor(value ? selectedColor : fontColor); }

    void prepareForReuse() override {
        this->image->setImageFromRes("pictures/video-card-bg.png");
        this->title->setText("");
        this->title->setTextColor(fontColor);
    }

    void cacheForReuse() override { ImageHelper::clear(this->image); }

    static RecyclingGridItem* create() { return new GridMainAreaCell(); }

protected:
    BRLS_BIND(brls::Label, title, "area/title");
    BRLS_BIND(brls::Image, image, "area/avatar");

    NVGcolor selectedColor = brls::Application::getTheme().getColor("color/tsvitch");
    NVGcolor fontColor     = brls::Application::getTheme().getColor("brls/text");
};

class DataSourceLiveVideoList : public RecyclingGridDataSource {
public:
    explicit DataSourceLiveVideoList(const tsvitch::LiveM3u8ListResult& result) : videoList(result) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        tsvitch::LiveM3u8& r = this->videoList[index];
        brls::Logger::info("cellForRow: {} [{}]", r.title, index);
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(r);
        return item;
    }

    size_t getItemCount() override { return videoList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        HistoryManager::get()->add(videoList[index]);
        Intent::openLive(videoList, index, [recycler]() { recycler->reloadData(); });
    }

    void appendData(const tsvitch::LiveM3u8ListResult& data) {
        this->videoList.insert(this->videoList.end(), data.begin(), data.end());
    }

    void clearData() override { this->videoList.clear(); }

private:
    tsvitch::LiveM3u8ListResult videoList;
};

HomeLive::HomeLive() {
    this->inflateFromXMLRes("xml/fragment/home_live.xml");
    brls::Logger::info("Fragment HomeLive: constructor called");
    
    // Inizializza il flag di validità
    validityFlag = std::make_shared<std::atomic<bool>>(true);
    
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    upRecyclingGrid->registerCell("Cell", []() { return DynamicGroupChannels::create(); });

    // Sottoscrivi all'evento di cambio M3U8
    OnM3U8UrlChanged.subscribe([this]() {
        brls::Logger::debug("OnM3U8UrlChanged: showing skeleton and requesting channel list");
        // Mostra lo skeleton per indicare che stiamo caricando
        brls::Threading::sync([this]() {
            recyclingGrid->showSkeleton();
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList();
        //reset index group
        this->selectGroupIndex(0);
    });

    // Sottoscrivi all'evento di cambio modalità IPTV
    OnIPTVModeChanged.subscribe([this]() {
        brls::Logger::debug("OnIPTVModeChanged: showing skeleton and requesting channel list");
        // Mostra lo skeleton per indicare che stiamo caricando
        brls::Threading::sync([this]() {
            recyclingGrid->showSkeleton();
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList();
        //reset index group
        this->selectGroupIndex(0);
    });

    // Sottoscrivi all'evento di cambio Xtream
    OnXtreamChanged.subscribe([this](const XtreamData& xtreamData) {
        brls::Logger::debug("OnXtreamChanged: url={}, username={}, showing skeleton and requesting channel list", 
                           xtreamData.url, xtreamData.username);
        // Mostra lo skeleton per indicare che stiamo caricando
        brls::Threading::sync([this]() {
            recyclingGrid->showSkeleton();
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList();
        //reset index group
        this->selectGroupIndex(0);
    });
    
    // Check if we're in Xtream mode and load channels immediately
    int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
    brls::Logger::info("HomeLive constructor: IPTV mode is {}", iptvMode);
    
    if (iptvMode == 1) {
        brls::Logger::info("HomeLive constructor: Xtream mode detected, loading channels immediately");
        // Clear cache first
        ChannelManager::get()->remove();
        // Load channels directly
        brls::Logger::info("HomeLive constructor: Requesting live list...");
        this->requestLiveList();
    } else {
        brls::Logger::debug("HomeLive constructor: M3U8 mode detected, will load cached channels or request new ones");
        // For M3U8 mode, try to load cached channels first
        auto cachedChannels = ChannelManager::get()->load();
        if (cachedChannels.empty()) {
            brls::Logger::debug("HomeLive constructor: No cached channels, requesting live list");
            this->requestLiveList();
        } else {
            brls::Logger::debug("HomeLive constructor: Found {} cached channels", cachedChannels.size());
            this->onLiveList(cachedChannels, false);
        }
    }
}

void HomeLive::onError(const std::string& error) {
    brls::Logger::error("Fragment HomeLive: onError: {}", error);
    brls::sync([this, error]() {
        this->recyclingGrid->setError(error);
        this->upRecyclingGrid->setVisibility(brls::Visibility::GONE);
    });

    //dialog to show error
    auto dialog = new brls::Dialog("hints/network_error"_i18n);
    dialog->addButton("hints/back"_i18n, []() {});
    dialog->open();
}

void HomeLive::onLiveList(const tsvitch::LiveM3u8ListResult& result, bool firstLoad) {
    brls::Logger::info("Fragment HomeLive: onLiveList - received {} channels", result.size());
    if (result.empty()) {
        recyclingGrid->setEmpty();
        upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        return;
    }

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [this](...) {
        if (isSearchActive) {
            this->cancelSearch();
        } else {
            auto dialog = new brls::Dialog("hints/exit_hint"_i18n);
            dialog->addButton("hints/cancel"_i18n, []() {});
            dialog->addButton("hints/ok"_i18n, []() { brls::Application::quit(); });
            dialog->open();
        }
        return true;
    });

    this->registerAction("hints/search"_i18n, brls::BUTTON_Y, [this](...) {
        this->search();
        return true;
    });

    this->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->toggleFavorite();
        return true;
    });

    this->registerAction("Scarica video", brls::BUTTON_RT, [this](...) {
        this->downloadVideo();
        return true;
    });

    if (!firstLoad) {
        brls::Threading::async([result] { ChannelManager::get()->save(result); });
    }

    // Raggruppa i canali per groupTitle SENZA COPIARE i dati
    std::map<std::string, std::vector<const tsvitch::LiveM3u8*>> groupMap;
    for (const auto& item : result) {
        groupMap[item.groupTitle].push_back(&item);
    }
    std::vector<std::string> groupTitles;
    for (const auto& item : groupMap) {
        groupTitles.push_back(item.first);
    }
    
    brls::Logger::info("HomeLive: Found {} groups", groupTitles.size());

    // Carica solo il gruppo selezionato in sync
    int lastIndex = ProgramConfig::instance().getSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);
    if (lastIndex >= groupTitles.size()) lastIndex = 0;
    std::string selectedGroup = groupTitles.empty() ? "" : groupTitles[lastIndex];

    tsvitch::LiveM3u8ListResult filtered;
    if (!selectedGroup.empty()) {
        for (const auto* ptr : groupMap[selectedGroup]) {
            filtered.push_back(*ptr);
        }
    }
    
    brls::Logger::info("HomeLive: Selected group '{}' with {} channels", selectedGroup, filtered.size());

    {
        std::lock_guard<std::mutex> lock(groupCacheMutex);
        groupCache.clear();
        groupCache[selectedGroup] = filtered;
    }
    this->channelsList = result;

    // Mostra solo il gruppo selezionato
    brls::Threading::sync([this, filtered]() {
        brls::Logger::info("HomeLive: Setting DataSource with {} filtered channels", filtered.size());
        if (filtered.empty())
            recyclingGrid->setEmpty();
        else
            recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));
    });

    // Precarica gli altri gruppi in background (senza bloccare la UI)
    // Copia result localmente per evitare problemi di concorrenza
    auto resultCopy = result;
    auto isValidFlag = validityFlag; // Cattura una copia del flag
    for (const auto& group : groupTitles) {
        if (group == selectedGroup) continue;
        // Copia direttamente gli oggetti, non i puntatori!
        tsvitch::LiveM3u8ListResult filteredBg;
        for (const auto& item : resultCopy) {
            if (item.groupTitle == group) filteredBg.push_back(item);
        }
        // Sposta la copia nel thread asincrono con una copia locale di `group`
        std::string groupCopy = group;
        brls::Threading::async([this, isValidFlag, groupCopy = std::move(groupCopy), filteredBg = std::move(filteredBg)]() mutable {
            // Check if this object is still valid before accessing it
            if (!isValidFlag->load()) {
                brls::Logger::debug("HomeLive::onLiveList: Object destroyed before async callback");
                return;
            }
            
            std::lock_guard<std::mutex> lock(groupCacheMutex);
            groupCache[groupCopy] = std::move(filteredBg);
        });
    }

    // UI gruppi
    brls::Threading::sync([this, groupTitles, lastIndex]() {
        if (groupTitles.size() == 1) {
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        } else {
            upRecyclingGrid->setVisibility(brls::Visibility::VISIBLE);
            auto* upList = new DataSourceUpList(groupTitles, [this](const std::string& group) {
                // Cambio gruppo: usa cache se presente, altrimenti filtra ora
                tsvitch::LiveM3u8ListResult filtered;
                {
                    std::lock_guard<std::mutex> lock(groupCacheMutex);
                    if (groupCache.count(group)) {
                        filtered = groupCache[group];
                    } else {
                        for (const auto& item : this->channelsList) {
                            if (item.groupTitle == group) filtered.push_back(item);
                        }
                        groupCache[group] = filtered;
                    }
                }
                if (filtered.empty())
                    recyclingGrid->setEmpty();
                else
                    recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));
            });
            upRecyclingGrid->setDataSource(upList);
            this->selectGroupIndex(static_cast<size_t>(lastIndex));
        }
    });
}
void HomeLive::selectGroupIndex(size_t index) {
    auto* datasource = dynamic_cast<DataSourceUpList*>(upRecyclingGrid->getDataSource());
    if (!datasource) return;
    if (index >= datasource->getItemCount()) return;
    this->selectedGroupIndex = index;
    datasource->setSelectedIndex(upRecyclingGrid, index);
    upRecyclingGrid->selectRowAt(index, false);

    std::string selectedGroup = datasource->getGroupNameByIndex(index);
    tsvitch::LiveM3u8ListResult filtered;
    {
        std::lock_guard<std::mutex> lock(groupCacheMutex);
        if (groupCache.count(selectedGroup)) {
            filtered = groupCache[selectedGroup];
        } else {
            for (const auto& item : this->channelsList) {
                if (item.groupTitle == selectedGroup) filtered.push_back(item);
            }
            groupCache[selectedGroup] = filtered;
        }
    }
    if (filtered.empty())
        recyclingGrid->setEmpty();
    else
        recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));

    brls::Logger::debug("selectGroupIndex: {}", index);
}

void HomeLive::toggleFavorite() {
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

void HomeLive::search() {
    brls::Application::getImeManager()->openForText([this](const std::string& text) { this->filter(text); },
                                                    "tsvitch/home/common/search"_i18n, "", 32, "", 0);
}

void HomeLive::cancelSearch() {
    isSearchActive = false;
    this->recyclingGrid->setDataSource(new DataSourceLiveVideoList(this->channelsList));
    upRecyclingGrid->setVisibility(brls::Visibility::VISIBLE);
    this->selectGroupIndex(this->selectedGroupIndex);
}

void HomeLive::filter(const std::string& key) {
    if (key.empty()) return;

    isSearchActive = true;

    brls::Threading::sync([this, key]() {
        auto* datasource = dynamic_cast<DataSourceLiveVideoList*>(recyclingGrid->getDataSource());
        if (datasource) {
            tsvitch::LiveM3u8ListResult filtered;
            std::string lowerKey = key;
            std::transform(lowerKey.begin(), lowerKey.end(), lowerKey.begin(),
                           [](unsigned char c) { return std::tolower(c); });
            for (const auto& item : this->channelsList) {
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
                recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));
            }
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        }
    });
}

void HomeLive::onShow() {
    brls::Logger::info("Fragment HomeLive: onShow called");
    
    // Carica i canali in background ogni volta che la vista viene mostrata
    brls::Threading::async([this] {
        // Se siamo in modalità Xtream, forza sempre l'aggiornamento per ora
        int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
        brls::Logger::debug("HomeLive onShow: Current IPTV mode: {}", iptvMode);
        
        if (iptvMode == 1) {
            brls::Logger::debug("HomeLive onShow: Xtream mode detected, forcing fresh data fetch");
            ChannelManager::get()->remove();
            brls::Logger::debug("HomeLive onShow: Cache cleared, requesting live list...");
            
            // Esegui requestLiveList in sync per essere sicuri che l'UI venga aggiornata
            brls::sync([this]() {
                brls::Logger::debug("HomeLive onShow: Calling requestLiveList()...");
                this->requestLiveList();
                brls::Logger::debug("HomeLive onShow: requestLiveList() called");
            });
            return;
        }
        
        auto cachedChannels = ChannelManager::get()->load();
        brls::sync([this, cachedChannels]() {
            if (cachedChannels.empty()) {
                brls::Logger::debug("HomeLive onShow: No cached channels found, requesting live list.");
                this->requestLiveList();
            } else {
                brls::Logger::debug("HomeLive onShow: Found {} cached channels, displaying them.", cachedChannels.size());
                this->onLiveList(cachedChannels, false);
            }
        });
    });
    
    brls::Logger::debug("HomeLive onShow: Reloading RecyclingGrid data...");
    this->recyclingGrid->reloadData();
    this->upRecyclingGrid->reloadData();
    brls::Logger::debug("HomeLive onShow: onShow completed");
}

void HomeLive::onCreate() {
    brls::Logger::debug("Fragment HomeLive: onCreate called");

    // Check if we're in Xtream mode and trigger loading immediately
    int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
    brls::Logger::debug("HomeLive onCreate: IPTV mode is {}", iptvMode);
    
    if (iptvMode == 1) {
        brls::Logger::debug("HomeLive onCreate: Xtream mode detected, requesting channels immediately");
        ChannelManager::get()->remove();
        this->requestLiveList();
    } else {
        brls::Logger::debug("HomeLive onCreate: M3U8 mode, loading from cache");
        auto cachedChannels = ChannelManager::get()->load();
        if (cachedChannels.empty()) {
            brls::Logger::debug("HomeLive onCreate: No cached channels, requesting live list");
            this->requestLiveList();
        } else {
            brls::Logger::debug("HomeLive onCreate: Found {} cached channels", cachedChannels.size());
            this->onLiveList(cachedChannels, false);
        }
    }

    // for (int i = 0; i < 100; ++i) {
    //     // Crea la sidebar item (puoi personalizzare label e stile)
    //    auto* item = new AutoSidebarItem();
    //         item->setTabStyle(AutoTabBarStyle::PLAIN);
    //         item->setLabel("Tab " + std::to_string(i + 1));
    //         item->setFontSize(18);

    //     // Funzione che crea la view associata al tab
    //        this->tabFrame->addTab(item, [this, i, item]() {
    //         // Qui puoi restituire una view diversa per ogni tab
    //         // Esempio: una semplice Box con un'etichetta
    //         auto* box = new brls::Box();
    //         auto* label = new brls::Label();
    //         label->setText("Contenuto Tab " + std::to_string(i + 1));
    //         box->addView(label);
    //         return box;
    //     });
    // }
}

HomeLive::~HomeLive() { 
    brls::Logger::debug("Fragment HomeLiveActivity: delete");
    // Invalidate the flag to prevent callbacks from accessing this object
    if (validityFlag) {
        validityFlag->store(false);
    }
}

brls::View* HomeLive::create() { 
    brls::Logger::debug("HomeLive::create() called - creating new HomeLive instance");
    return new HomeLive(); 
}

void HomeLive::downloadVideo() {
    // Ottieni l'item attualmente focalizzato
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeLive::downloadVideo: No focused item");
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
            // Callback di progresso - aggiorna il manager globale
            std::string progressText = fmt::format("{:.1f}%", progress);
            std::string statusText = fmt::format("{} / {} bytes", downloaded, total);
            
            brls::sync([id, progress, progressText, statusText]() {
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    id, progress, statusText, progressText
                );
            });
            
            brls::Logger::debug("Download {}: {:.1f}% ({}/{} bytes)", id, progress, downloaded, total);
        },
        [](const std::string& id, const std::string& filePath) {
            // Callback di completamento
            brls::Logger::info("Download {} completed: {}", id, filePath);
            brls::sync([id]() {
                // Nascondi l'overlay
                tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                brls::Application::notify("Download completato!");
            });
        },
        [](const std::string& id, const std::string& error) {
            // Callback di errore
            brls::Logger::error("Download {} failed: {}", id, error);
            brls::sync([id, error]() {
                // Nascondi l'overlay
                tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                brls::Application::notify("Errore download: " + error);
            });
        }
    );
    
    if (!downloadId.empty()) {
        // Mostra l'overlay globale
        tsvitch::DownloadProgressManager::getInstance()->showDownloadProgress(
            downloadId, channel.title, channel.url
        );
        
        brls::Application::notify("Download avviato: " + channel.title);
        brls::Logger::info("HomeLive: Started download {} for {}", downloadId, channel.title);
    } else {
        brls::Application::notify("Errore nell'avvio del download");
        brls::Logger::error("HomeLive: Failed to start download for {}", channel.title);
    }
}