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
#include "view/selector_cell.hpp"

#include "core/HistoryManager.hpp"
#include "core/FavoriteManager.hpp"
#include "core/ChannelManager.hpp"
#include "core/DownloadManager.hpp"
#include "utils/stream_helper.hpp"
#include "utils/download_helper.hpp"
#include "core/DownloadProgressManager.hpp"

#include "utils/config_helper.hpp"

using namespace brls::literals;

#ifdef __SWITCH__
static constexpr size_t SWITCH_XTREAM_VOD_UI_LIMIT = 300;

static void limitSwitchVodGroupForUi(tsvitch::LiveM3u8ListResult& list, int contentType, const std::string& context) {
    brls::Logger::info("HomeLive: Switch {} UI limit check contentType={} size={}",
                       context, contentType, list.size());
    if (list.size() <= SWITCH_XTREAM_VOD_UI_LIMIT) return;

    brls::Logger::warning("HomeLive: Switch {} UI limit: reducing group render from {} to {} items",
                          context, list.size(), SWITCH_XTREAM_VOD_UI_LIMIT);
    list.resize(SWITCH_XTREAM_VOD_UI_LIMIT);
}
#endif

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
        if (!recycler) {
            brls::Logger::error("DataSourceLiveVideoList: cellForRow recycler is null");
            return nullptr;
        }
        if (index >= this->videoList.size()) {
            brls::Logger::error("DataSourceLiveVideoList: cellForRow index {} out of bounds size={}",
                                index, this->videoList.size());
            return nullptr;
        }
#ifdef __SWITCH__
        if (index % 50 == 0) {
            brls::Logger::info("DataSourceLiveVideoList: Switch cellForRow index={} size={}", index, this->videoList.size());
        }
#endif
        tsvitch::LiveM3u8& r = this->videoList[index];
        // brls::Logger::info("cellForRow: {} [{}]", r.title, index);
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        if (!item) {
            brls::Logger::error("DataSourceLiveVideoList: dequeueReusableCell returned null at index {}", index);
            return nullptr;
        }
        item->setChannel(r, index);
        return item;
    }

    size_t getItemCount() override { return videoList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        brls::Logger::info("DataSourceLiveVideoList: onItemSelected index={} size={}", index, videoList.size());
        if (index >= videoList.size()) {
            brls::Logger::warning("DataSourceLiveVideoList: selection ignored, index out of bounds");
            return;
        }
        HistoryManager::get()->add(videoList[index]);
        brls::Logger::info("DataSourceLiveVideoList: opening live title='{}' url_empty={}",
                           videoList[index].title, videoList[index].url.empty());
#ifdef __SWITCH__
        if (!tsvitch::isLiveStream(videoList[index].url, videoList[index].title)) {
            if (tsvitch::isXtreamSeriesPlaceholder(videoList[index].url)) {
                brls::Logger::info("DataSourceLiveVideoList: Switch series placeholder opens episode picker");
                Intent::openLive(videoList, index, [recycler]() { recycler->reloadData(); });
            } else {
                brls::Logger::info("DataSourceLiveVideoList: Switch VOD selection no-op during crash isolation");
            }
            return;
        }
#endif
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
    this->xtreamContentType = ProgramConfig::instance().getSettingItem(SettingItem::XTREAM_CONTENT_TYPE, 0);
    
    // Sottoscrivi all'evento di uscita per cancellare tutti i task asincroni
    exitEventSubscription = brls::Application::getExitEvent()->subscribe([this]() {
        brls::Logger::info("HomeLive: Exit event received, canceling all async operations");
        if (validityFlag) {
            validityFlag->store(false);
        }
    });
    hasExitSubscription = true;
    
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    upRecyclingGrid->registerCell("Cell", []() { return DynamicGroupChannels::create(); });

    // Registra il handler del tasto B nel costruttore (sempre attivo)
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

    // Sottoscrivi all'evento di cambio M3U8
    OnM3U8UrlChanged.subscribe([this]() {
        brls::Logger::debug("OnM3U8UrlChanged: showing skeleton and requesting channel list");
        // Mostra lo skeleton per indicare che stiamo caricando
        brls::Threading::sync([this]() {
            recyclingGrid->showSkeleton();
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
            contentTypeSelector->setVisibility(brls::Visibility::GONE);
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList(0);
        //reset index group
        this->selectGroupIndex(0);
    });

    // Sottoscrivi all'evento di cambio modalità IPTV
    OnIPTVModeChanged.subscribe([this]() {
        brls::Logger::debug("OnIPTVModeChanged: showing skeleton and requesting channel list");
        int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
        brls::Threading::sync([this, iptvMode]() {
            recyclingGrid->showSkeleton();
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
            if (iptvMode == 0) {
                contentTypeSelector->setVisibility(brls::Visibility::GONE);
            } else {
                contentTypeSelector->setVisibility(brls::Visibility::VISIBLE);
                std::vector<std::string> options = {"Live TV", "Movies", "Series"};
                contentTypeSelector->init("Content Type", options, xtreamContentType, [this](int selection) {
                    this->handleXtreamContentTypeSelection(selection);
                });
            }
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList(iptvMode == 1 ? this->xtreamContentType : 0);
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
            contentTypeSelector->setVisibility(brls::Visibility::VISIBLE);
            std::vector<std::string> options = {"Live TV", "Movies", "Series"};
            contentTypeSelector->init("Content Type", options, xtreamContentType, [this](int selection) {
                this->handleXtreamContentTypeSelection(selection);
            });
        });
        
        ChannelManager::get()->remove();
        this->requestLiveList(this->xtreamContentType);
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
    
    // Configure content type selector visibility and callback
    if (iptvMode == 0) {
        contentTypeSelector->setVisibility(brls::Visibility::GONE);
    } else {
        contentTypeSelector->setVisibility(brls::Visibility::VISIBLE);
        std::vector<std::string> options = {"Live TV", "Movies", "Series"};
        contentTypeSelector->init("Content Type", options, xtreamContentType, [this](int selection) {
            this->handleXtreamContentTypeSelection(selection);
        });
    }
    
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
                    this->requestLiveList(this->xtreamContentType);
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
                    this->requestLiveList(0);
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

void HomeLive::handleXtreamContentTypeSelection(int selection) {
    if (this->xtreamContentType == selection) {
        brls::Logger::info("HomeLive: Xtream content type unchanged ({})", selection);
        return;
    }

    brls::Logger::info("HomeLive: Xtream content type change requested: {} -> {}", this->xtreamContentType, selection);
    this->xtreamContentType = selection;
    this->selectedGroupIndex = 0;
    ProgramConfig::instance().setSettingItem(SettingItem::XTREAM_CONTENT_TYPE, selection);
    ProgramConfig::instance().setSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);

    if (validityFlag) validityFlag->store(false);
    validityFlag = std::make_shared<std::atomic<bool>>(true);
    auto requestFlag = validityFlag;

    brls::Threading::sync([this]() {
        brls::Logger::info("HomeLive: preparing UI for deferred Xtream content type reload");
        recyclingGrid->showSkeleton();
        upRecyclingGrid->setVisibility(brls::Visibility::GONE);
    });

#ifdef __SWITCH__
    brls::Logger::info("HomeLive: deferring Xtream content type request on Switch");
    brls::Threading::delay(350, [this, requestFlag, selection]() {
        if (!requestFlag || !requestFlag->load()) {
            brls::Logger::info("HomeLive: deferred Xtream content type request canceled");
            return;
        }
        brls::Logger::info("HomeLive: running deferred Xtream content type request {}", selection);
        ChannelManager::get()->remove();
        this->requestLiveList(selection);
    });
#else
    ChannelManager::get()->remove();
    this->requestLiveList(selection);
#endif
}

void HomeLive::onLiveList(tsvitch::LiveM3u8ListResult result, bool firstLoad) {
    brls::Logger::info("Fragment HomeLive: onLiveList - received {} channels", result.size());
    if (result.empty()) {
        recyclingGrid->setEmpty();
        upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        return;
    }

    this->registerAction("hints/search"_i18n, brls::BUTTON_Y, [this](...) {
        this->search();
        return true;
    });

    this->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        brls::Logger::info("HomeLive: BUTTON_X toggle favorite action");
        this->toggleFavorite();
        return true;
    });

#ifndef __SWITCH__
    this->registerAction("Scarica", brls::BUTTON_RT, [this](...) {
        brls::Logger::info("HomeLive: BUTTON_RT download action");
        this->downloadVideo();
        return true;
    });
#endif

    // Salva channelsList SUBITO per accesso thread-safe
    this->channelsList = std::move(result); // Move invece di copy!
    
    // Fai il grouping e UI update SUL MAIN THREAD per evitare il delay di 36s del brls::sync()
    // Meglio bloccare 600ms che aspettare 36 secondi!
    auto isValidFlag = validityFlag;
    brls::sync([this, isValidFlag, firstLoad]() {
        if (!isValidFlag->load()) return;
        
        auto grouping_start = std::chrono::high_resolution_clock::now();
        
        // Raggruppa i canali per groupTitle - unica passata
        std::unordered_map<std::string, std::vector<size_t>> groupIndices;
        groupIndices.reserve(100);
        
        for (size_t i = 0; i < this->channelsList.size(); ++i) {
            std::string groupTitle = this->channelsList[i].groupTitle;
            // Assegna un nome di default ai canali senza gruppo
            if (groupTitle.empty()) {
                groupTitle = "Uncategorized";
            }
            groupIndices[groupTitle].push_back(i);
        }
        
        std::vector<std::string> groupTitles;
        groupTitles.reserve(groupIndices.size());
        for (const auto& pair : groupIndices) {
            groupTitles.push_back(pair.first);
        }
        
        // Ordina alfabeticamente
        std::sort(groupTitles.begin(), groupTitles.end());
        
        auto grouping_end = std::chrono::high_resolution_clock::now();
        auto grouping_duration = std::chrono::duration_cast<std::chrono::milliseconds>(grouping_end - grouping_start);
        brls::Logger::info("HomeLive: Grouping completed in {}ms - Found {} groups", grouping_duration.count(), groupTitles.size());
        
        // Leggi lastIndex dal config
        int lastIndex = ProgramConfig::instance().getSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);
        if (lastIndex >= (int)groupTitles.size()) lastIndex = 0;
        std::string selectedGroup = groupTitles.empty() ? "" : groupTitles[lastIndex];
        
        // Prepara il gruppo selezionato
        tsvitch::LiveM3u8ListResult filtered;
        if (!selectedGroup.empty() && groupIndices.count(selectedGroup)) {
            const auto& indices = groupIndices[selectedGroup];
            filtered.reserve(indices.size());
            for (size_t idx : indices) {
                filtered.push_back(this->channelsList[idx]);
            }
        }
        
        brls::Logger::info("HomeLive: Selected group '{}' with {} channels", selectedGroup, filtered.size());
#ifdef __SWITCH__
        limitSwitchVodGroupForUi(filtered, this->xtreamContentType, "initial group");
#endif
        
        // Cache il gruppo selezionato
        {
            std::lock_guard<std::mutex> lock(groupCacheMutex);
            groupCache.clear();
            groupCache[selectedGroup] = filtered;
        }
        
        // Imposta il DataSource principale (già sul main thread, no brls::sync necessario)
        brls::Logger::info("HomeLive: Setting DataSource with {} filtered channels", filtered.size());
        if (filtered.empty())
            recyclingGrid->setEmpty();
        else
            recyclingGrid->setDataSource(new DataSourceLiveVideoList(std::move(filtered)));
        
        // Setup UI gruppi
        if (groupTitles.size() > 1) {
            upRecyclingGrid->setVisibility(brls::Visibility::VISIBLE);
            auto* upList = new DataSourceUpList(groupTitles, [this](const std::string& group) {
                brls::Logger::info("HomeLive: group selected '{}'", group);
                tsvitch::LiveM3u8ListResult filtered;
                {
                    std::lock_guard<std::mutex> lock(groupCacheMutex);
                    if (groupCache.count(group)) {
                        filtered = groupCache[group];
                        brls::Logger::debug("HomeLive: Using cached group '{}' with {} channels", group, filtered.size());
                    } else {
                        brls::Logger::warning("HomeLive: Cache miss for group '{}', filtering on-demand", group);
                        for (const auto& item : this->channelsList) {
                            if (item.groupTitle == group) filtered.push_back(item);
                        }
                        groupCache[group] = filtered;
                    }
                }
#ifdef __SWITCH__
                limitSwitchVodGroupForUi(filtered, this->xtreamContentType, "selected group");
#endif
                if (filtered.empty())
                    recyclingGrid->setEmpty();
                else
                    recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));
            });
            upRecyclingGrid->setDataSource(upList);
            this->selectGroupIndex(static_cast<size_t>(lastIndex));
        } else {
            upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        }
        
        // Precarica gli altri gruppi in background - IN UN THREAD ASYNC SEPARATO per non bloccare l'UI
        brls::Threading::async([this, groupTitles = std::move(groupTitles), groupIndices = std::move(groupIndices), 
                                selectedGroup, isValidFlag]() {
            if (groupTitles.size() <= 1) return;
            
            auto preload_start = std::chrono::high_resolution_clock::now();
            size_t groupsProcessed = 0;
            
            for (const auto& group : groupTitles) {
                if (!isValidFlag->load()) return;
                if (group == selectedGroup) continue;
                
                tsvitch::LiveM3u8ListResult filteredBg;
                if (groupIndices.count(group)) {
                    const auto& indices = groupIndices.at(group);
                    filteredBg.reserve(indices.size());
                    
                    for (size_t idx : indices) {
                        if (idx < this->channelsList.size()) {
                            filteredBg.push_back(this->channelsList[idx]);
                        }
                    }
                }
                
                {
                    std::lock_guard<std::mutex> lock(groupCacheMutex);
                    groupCache[group] = std::move(filteredBg);
                }
                
                groupsProcessed++;
                if (groupsProcessed % 10 == 0) {
                    std::this_thread::yield();
                }
            }
            
            auto preload_end = std::chrono::high_resolution_clock::now();
            auto preload_duration = std::chrono::duration_cast<std::chrono::milliseconds>(preload_end - preload_start);
            brls::Logger::info("HomeLive: Background group preloading completed in {}ms ({} groups)", preload_duration.count(), groupsProcessed);
        });
        
        // Salva in background se firstLoad (non blocca UI)
        if (firstLoad) {
            brls::Logger::info("HomeLive: First load detected, will save {} channels with timestamp (async)", this->channelsList.size());
            auto toSave = this->channelsList;
            brls::Threading::async([data = std::move(toSave)]() {
                try {
                    ChannelManager::get()->saveWithTimestamp(data);
                    brls::Logger::info("HomeLive: Async saveWithTimestamp completed successfully");
                } catch (const std::exception& e) {
                    brls::Logger::error("HomeLive: Exception in async saveWithTimestamp: {}", e.what());
                } catch (...) {
                    brls::Logger::error("HomeLive: Unknown exception in async saveWithTimestamp");
                }
            });
        }
    });
}

void HomeLive::selectGroupIndex(size_t index) {
    brls::Logger::info("HomeLive: selectGroupIndex begin index={}", index);
    auto* datasource = dynamic_cast<DataSourceUpList*>(upRecyclingGrid->getDataSource());
    if (!datasource) {
        brls::Logger::warning("HomeLive: selectGroupIndex ignored, datasource is null");
        return;
    }
    if (index >= datasource->getItemCount()) {
        brls::Logger::warning("HomeLive: selectGroupIndex ignored, index={} itemCount={}", index, datasource->getItemCount());
        return;
    }
    this->selectedGroupIndex = index;
    datasource->setSelectedIndex(upRecyclingGrid, index);
    brls::Logger::info("HomeLive: selectGroupIndex after setSelectedIndex index={}", index);
    upRecyclingGrid->selectRowAt(index, false);
    brls::Logger::info("HomeLive: selectGroupIndex after selectRowAt index={}", index);

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
    else {
#ifdef __SWITCH__
        limitSwitchVodGroupForUi(filtered, this->xtreamContentType, "selectGroupIndex");
#endif
        recyclingGrid->setDataSource(new DataSourceLiveVideoList(filtered));
    }

    brls::Logger::info("HomeLive: selectGroupIndex done index={} filtered={}", index, filtered.size());
}

void HomeLive::toggleFavorite() {
    //get focus item
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeLive::toggleFavorite: no focused video card");
        return;
    }

    //get channel
    tsvitch::LiveM3u8 channel = item->getChannel();
    brls::Logger::info("HomeLive::toggleFavorite: title='{}' url_empty={}", channel.title, channel.url.empty());

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
#ifdef __SWITCH__
        brls::Logger::info("HomeLive onShow: Switch keeps existing {} in-memory channels, skipping automatic network refresh", channelsList.size());
        this->recyclingGrid->reloadData();
        this->upRecyclingGrid->reloadData();
        return;
#else
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
                    this->requestLiveList(iptvMode == 1 ? this->xtreamContentType : 0);
                } else {
                    brls::Logger::debug("HomeLive onShow: Cache still valid, no refresh needed");
                    // Solo ricarica i dati delle grid per aggiornare la UI
                    this->recyclingGrid->reloadData();
                    this->upRecyclingGrid->reloadData();
                }
            });
        });
        return;
#endif
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
        
        brls::sync([this, cachedChannels, validityFlag, iptvMode]() {
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
                this->requestLiveList(iptvMode == 1 ? this->xtreamContentType : 0);
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
    
    // Cancella la sottoscrizione all'evento di uscita solo se è stata creata
    if (hasExitSubscription) {
        brls::Application::getExitEvent()->unsubscribe(exitEventSubscription);
    }
    
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
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeLive::downloadVideo: No focused item");
        return;
    }

    auto channel = item->getChannel();
    brls::Logger::info("HomeLive::downloadVideo: title='{}' url_empty={}", channel.title, channel.url.empty());
    tsvitch::startChannelDownloadWithUi(channel, "HomeLive");
}
