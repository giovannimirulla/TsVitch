#include <utility>
#include <unordered_map>
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
    
    // Sottoscrivi all'evento di uscita per cancellare tutti i task asincroni
    exitEventSubscription = brls::Application::getExitEvent()->subscribe([this]() {
        brls::Logger::info("HomeLive: Exit event received, canceling all async operations");
        if (validityFlag) {
            validityFlag->store(false);
        }
    });
    
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
    
    // Mostra sempre lo skeleton all'inizio per UI non-bloccante
    brls::Logger::debug("HomeLive constructor: Showing skeleton for non-blocking UI");
    recyclingGrid->showSkeleton();
    upRecyclingGrid->setVisibility(brls::Visibility::GONE);
    
    // Imposta il flag per indicare che il caricamento è in corso
    isInitialLoadInProgress = true;
    
    // Check if we're in Xtream mode and load channels immediately
    int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
    brls::Logger::info("HomeLive constructor: IPTV mode is {}", iptvMode);
    
    if (iptvMode == 1) {
        brls::Logger::info("HomeLive constructor: Xtream mode detected, using smart cache approach");
        
        // Prova prima la cache intelligente anche per Xtream
        brls::Threading::async([this, validityFlag = this->validityFlag] {
            // Controlla se l'app è ancora valida prima di procedere
            if (!validityFlag || !validityFlag->load()) {
                brls::Logger::debug("HomeLive: Xtream async task canceled - app exiting");
                return;
            }
            
            auto cachedChannels = ChannelManager::get()->loadIfValid(); 
            
            brls::sync([this, cachedChannels, validityFlag]() {
                // Controlla di nuovo la validità prima di aggiornare l'UI
                if (!validityFlag || !validityFlag->load()) {
                    brls::Logger::debug("HomeLive: Xtream sync task canceled - app exiting");
                    return;
                }
                
                if (!cachedChannels.empty()) {
                    brls::Logger::info("HomeLive: Using valid Xtream cache with {} channels", cachedChannels.size());
                    this->onLiveList(cachedChannels, false);
                } else {
                    brls::Logger::info("HomeLive: Xtream cache invalid/empty, requesting fresh data");
                    this->requestLiveList();
                }
                isInitialLoadInProgress = false; // Reset flag quando completato
            });
        });
    } else {
        brls::Logger::debug("HomeLive constructor: M3U8 mode detected, will use intelligent caching");
        
        // Per M3U8 mode, usa cache intelligente con timeout più lungo
        brls::Threading::async([this, validityFlag = this->validityFlag] {
            // Controlla se l'app è ancora valida prima di procedere
            if (!validityFlag || !validityFlag->load()) {
                brls::Logger::debug("HomeLive: M3U8 async task canceled - app exiting");
                return;
            }
            
            brls::Logger::debug("HomeLive: Starting smart cache check in background thread");
            
            // Cache più lunga per M3U8 (1 mese) perché cambia meno frequentemente
            auto cachedChannels = ChannelManager::get()->loadIfValid();
            brls::Logger::info("HomeLive: Smart cache check completed, found {} channels", cachedChannels.size());
            
            brls::sync([this, cachedChannels, validityFlag]() {
                // Controlla di nuovo la validità prima di aggiornare l'UI
                if (!validityFlag || !validityFlag->load()) {
                    brls::Logger::debug("HomeLive: M3U8 sync task canceled - app exiting");
                    return;
                }
                
                if (!cachedChannels.empty()) {
                    brls::Logger::info("HomeLive constructor: Using valid M3U8 cache ({} channels found)", cachedChannels.size());
                    this->onLiveList(cachedChannels, false);
                } else {
                    brls::Logger::info("HomeLive constructor: M3U8 cache is invalid or empty, requesting fresh channels");
                    this->requestLiveList();
                }
                isInitialLoadInProgress = false; // Reset flag quando completato
            });
        });
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

    if (firstLoad) {
        // Salva con timestamp solo quando è un caricamento fresco (non da cache)
        brls::Logger::info("HomeLive: First load detected, will save {} channels with timestamp", result.size());
        
        // Salva immediatamente per assicurarci che non si perda
        try {
            ChannelManager::get()->saveWithTimestamp(result);
            brls::Logger::info("HomeLive: Immediate saveWithTimestamp completed successfully");
        } catch (const std::exception& e) {
            brls::Logger::error("HomeLive: Exception in immediate saveWithTimestamp: {}", e.what());
        } catch (...) {
            brls::Logger::error("HomeLive: Unknown exception in immediate saveWithTimestamp");
        }
    } else {
        brls::Logger::info("HomeLive: Not first load ({}), skipping save", firstLoad);
    }

    // Raggruppa i canali per groupTitle OTTIMIZZATO - un'unica passata
    auto grouping_start = std::chrono::high_resolution_clock::now();
    
    std::unordered_map<std::string, std::vector<size_t>> groupIndices; // Usa indici invece di puntatori
    groupIndices.reserve(100); // Stima ragionevole del numero di gruppi
    
    for (size_t i = 0; i < result.size(); ++i) {
        const std::string& groupTitle = result[i].groupTitle;
        groupIndices[groupTitle].push_back(i);
    }
    
    std::vector<std::string> groupTitles;
    groupTitles.reserve(groupIndices.size());
    for (const auto& pair : groupIndices) {
        groupTitles.push_back(pair.first);
    }
    
    // Ordina i gruppi alfabeticamente per UX migliore
    std::sort(groupTitles.begin(), groupTitles.end());
    
    auto grouping_end = std::chrono::high_resolution_clock::now();
    auto grouping_duration = std::chrono::duration_cast<std::chrono::milliseconds>(grouping_end - grouping_start);
    brls::Logger::info("HomeLive: Grouping completed in {}ms - Found {} groups", grouping_duration.count(), groupTitles.size());

    // Carica solo il gruppo selezionato in sync
    int lastIndex = ProgramConfig::instance().getSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);
    if (lastIndex >= groupTitles.size()) lastIndex = 0;
    std::string selectedGroup = groupTitles.empty() ? "" : groupTitles[lastIndex];

    tsvitch::LiveM3u8ListResult filtered;
    if (!selectedGroup.empty() && groupIndices.count(selectedGroup)) {
        const auto& indices = groupIndices[selectedGroup];
        filtered.reserve(indices.size());
        for (size_t idx : indices) {
            filtered.push_back(result[idx]);
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

    // Precarica gli altri gruppi in background OTTIMIZZATO - parallelo e batch
    if (groupTitles.size() > 1) {
        auto resultCopy = result; // Copia per thread safety
        auto isValidFlag = validityFlag; 
        
        brls::Threading::async([this, isValidFlag, groupTitles, groupIndices, resultCopy = std::move(resultCopy), selectedGroup]() {
            if (!isValidFlag->load()) return;
            
            auto preload_start = std::chrono::high_resolution_clock::now();
            size_t groupsProcessed = 0;
            
            for (const auto& group : groupTitles) {
                if (!isValidFlag->load()) return; // Check periodico
                if (group == selectedGroup) continue;
                
                tsvitch::LiveM3u8ListResult filteredBg;
                if (groupIndices.count(group)) {
                    const auto& indices = groupIndices.at(group);
                    filteredBg.reserve(indices.size());
                    for (size_t idx : indices) {
                        filteredBg.push_back(resultCopy[idx]);
                    }
                }
                
                {
                    std::lock_guard<std::mutex> lock(groupCacheMutex);
                    groupCache[group] = std::move(filteredBg);
                }
                
                groupsProcessed++;
                
                // Yielding periodico per non monopolizzare il thread
                if (groupsProcessed % 5 == 0) {
                    std::this_thread::yield();
                }
            }
            
            auto preload_end = std::chrono::high_resolution_clock::now();
            auto preload_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preload_end - preload_start);
            brls::Logger::info("HomeLive: Background group preloading completed in {}ms ({} groups)", preload_duration.count(), groupsProcessed);
        });
    }

    // UI gruppi
    brls::Threading::sync([this, groupTitles, lastIndex]() {
        if (groupTitles.size() <= 1) {
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
                        brls::Logger::debug("HomeLive: Using cached group '{}' with {} channels", group, filtered.size());
                    } else {
                        // Fallback: filtra al volo (non dovrebbe succedere spesso)
                        brls::Logger::warning("HomeLive: Cache miss for group '{}', filtering on-demand", group);
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
    
    // Se il caricamento iniziale è ancora in corso, non fare nulla
    if (isInitialLoadInProgress) {
        brls::Logger::debug("HomeLive onShow: Initial load still in progress, skipping");
        return;
    }
    
    // Smart refresh: controlla se abbiamo già canali in memoria
    if (!channelsList.empty()) {
        brls::Logger::debug("HomeLive onShow: Already have {} channels in memory, checking if refresh needed", channelsList.size());
        
        // Per decidere se ricaricare, controlla l'età della cache
        int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
        int maxCacheAge = (iptvMode == 1) ? 5 : 15; // Xtream: 5 min, M3U8: 15 min
        
        brls::Threading::async([this, maxCacheAge, iptvMode, validityFlag = this->validityFlag] {
            // Controlla se l'app è ancora valida prima di procedere
            if (!validityFlag || !validityFlag->load()) {
                brls::Logger::debug("HomeLive onShow: async task canceled - app exiting");
                return;
            }
            
            bool needsRefresh = !ChannelManager::get()->isCacheValid(maxCacheAge);
            
            brls::sync([this, needsRefresh, iptvMode, validityFlag]() {
                // Controlla di nuovo la validità prima di aggiornare l'UI
                if (!validityFlag || !validityFlag->load()) {
                    brls::Logger::debug("HomeLive onShow: sync task canceled - app exiting");
                    return;
                }
                
                if (needsRefresh) {
                    brls::Logger::info("HomeLive onShow: Cache expired, refreshing channels (IPTV mode: {})", iptvMode);
                    this->requestLiveList();
                } else {
                    brls::Logger::debug("HomeLive onShow: Cache still valid, no refresh needed");
                    // Solo ricarica i dati delle grid per aggiornare la UI
                    this->recyclingGrid->reloadData();
                    this->upRecyclingGrid->reloadData();
                }
            });
        });
        return;
    }
    
    // Se non abbiamo canali e il caricamento iniziale non è in corso, usa lo stesso meccanismo del costruttore
    brls::Logger::debug("HomeLive onShow: No channels in memory and no initial load in progress, loading...");
    
    int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
    brls::Threading::async([this, iptvMode, validityFlag = this->validityFlag] {
        // Controlla se l'app è ancora valida prima di procedere
        if (!validityFlag || !validityFlag->load()) {
            brls::Logger::debug("HomeLive onShow: fallback async task canceled - app exiting");
            return;
        }
        
        auto cachedChannels = ChannelManager::get()->loadIfValid();
        
        brls::sync([this, cachedChannels, validityFlag]() {
            // Controlla di nuovo la validità prima di aggiornare l'UI
            if (!validityFlag || !validityFlag->load()) {
                brls::Logger::debug("HomeLive onShow: fallback sync task canceled - app exiting");
                return;
            }
            
            if (!cachedChannels.empty()) {
                brls::Logger::info("HomeLive onShow: Using valid cached channels ({} channels)", cachedChannels.size());
                this->onLiveList(cachedChannels, false);
            } else {
                brls::Logger::info("HomeLive onShow: No valid cache, requesting fresh channels");
                this->requestLiveList();
            }
        });
    });
    
    brls::Logger::debug("HomeLive onShow: onShow completed");
}

void HomeLive::onCreate() {
    brls::Logger::debug("Fragment HomeLive: onCreate called");

    // Non fare niente qui - il caricamento è già gestito nel costruttore
    // in modo completamente asincrono per evitare blocchi dell'UI
    brls::Logger::debug("HomeLive onCreate: Delegating to constructor for async loading");
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

HomeLive::~HomeLive() { 
    brls::Logger::debug("Fragment HomeLiveActivity: delete");
    
    // Cancella la sottoscrizione all'evento di uscita
    brls::Application::getExitEvent()->unsubscribe(exitEventSubscription);
    
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