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
#include "utils/stream_helper.hpp"
#include "core/DownloadProgressManager.hpp"

#include "utils/config_helper.hpp"
#include "tsvitch.h"

#include <borealis/core/box.hpp>
#include <borealis/core/i18n.hpp>
#include <borealis/views/label.hpp>

using namespace brls::literals;

// Sentinel scheme for series items (must match the one used in the API)
static const std::string XTREAM_SERIES_SCHEME = "xtream-series://";
// Handler invoked when a series is selected in the list (opens its episodes).
// Active only while the series list is visible; null in every other mode.
static std::function<void(const tsvitch::LiveM3u8&)> g_xtreamSeriesHandler = nullptr;

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
        // brls::Logger::info("cellForRow: {} [{}]", r.title, index);
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(r);
        return item;
    }

    size_t getItemCount() override { return videoList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        const tsvitch::LiveM3u8& item = videoList[index];
        // Series items (sentinel url) open the episodes instead of playing
        if (g_xtreamSeriesHandler && item.url.rfind(XTREAM_SERIES_SCHEME, 0) == 0) {
            g_xtreamSeriesHandler(item);
            return;
        }
        // Não registra no histórico conteúdo de categoria adulta (privacidade)
        if (!ProgramConfig::instance().isAdultCategory(item.groupTitle)) {
            HistoryManager::get()->add(item);
        }
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
    hasExitSubscription = true;
    
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    upRecyclingGrid->registerCell("Cell", []() { return DynamicGroupChannels::create(); });

    isXtreamMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0) == 1;

    // Hub cards (Live TV / Movies / Series) and back button — Xtream mode only
    hubLive->registerClickAction([this](brls::View*) { this->enterContentType(0); return true; });
    hubMovies->registerClickAction([this](brls::View*) { this->enterContentType(1); return true; });
    hubSeries->registerClickAction([this](brls::View*) { this->enterContentType(2); return true; });
    backButton->registerClickAction([this](brls::View*) { this->showContentHub(); return true; });

    // Enable touch/tap on the cards and the back button (gamepad click alone is not enough)
    hubLive->addGestureRecognizer(new brls::TapGestureRecognizer(hubLive));
    hubMovies->addGestureRecognizer(new brls::TapGestureRecognizer(hubMovies));
    hubSeries->addGestureRecognizer(new brls::TapGestureRecognizer(hubSeries));
    backButton->addGestureRecognizer(new brls::TapGestureRecognizer(backButton));

    // Botão de atualizar (limpa o cache do modo atual e recarrega do servidor)
    refreshButton->registerClickAction([this](brls::View*) { this->refreshCurrent(); return true; });
    refreshButton->addGestureRecognizer(new brls::TapGestureRecognizer(refreshButton));

    // Source reload: in Xtream go back to the hub, in M3U8 reload directly
    auto reloadOnSourceChange = [this]() {
        isXtreamMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0) == 1;
        ChannelManager::get()->remove();
        if (isXtreamMode) {
            this->showContentHub();
        } else {
            brls::Threading::sync([this]() {
                recyclingGrid->showSkeleton();
                upRecyclingGrid->setVisibility(brls::Visibility::GONE);
            });
            this->requestLiveList();
            this->selectGroupIndex(0);
        }
    };
    OnM3U8UrlChanged.subscribe(reloadOnSourceChange);
    OnIPTVModeChanged.subscribe(reloadOnSourceChange);
    OnXtreamChanged.subscribe([reloadOnSourceChange](const XtreamData&) { reloadOnSourceChange(); });

    if (isXtreamMode) {
        // In Xtream mode, entry shows the 3-card hub (nothing loads until a choice is made)
        brls::Logger::info("HomeLive constructor: Xtream mode -> showing content hub");
        this->showContentHub();
        isInitialLoadInProgress = false;
    } else {
        // M3U8: skeleton + smart cache (original behavior)
        brls::Logger::debug("HomeLive constructor: M3U8 mode, using intelligent caching");
        recyclingGrid->showSkeleton();
        upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        isInitialLoadInProgress = true;

        brls::Threading::async([this, validityFlag = this->validityFlag] {
            if (!validityFlag || !validityFlag->load()) return;

            auto cachedChannels = ChannelManager::get()->loadIfValid();
            brls::Logger::info("HomeLive: Smart cache check completed, found {} channels", cachedChannels.size());

            brls::sync([this, cachedChannels, validityFlag]() {
                if (!validityFlag || !validityFlag->load()) return;

                if (!cachedChannels.empty()) {
                    this->onLiveList(cachedChannels, false);
                } else {
                    this->requestLiveList();
                }
                isInitialLoadInProgress = false;
            });
        });
    }
}

void HomeLive::showContentHub() {
    if (!isXtreamMode) return;
    inHubMode      = true;
    isSearchActive = false;

    leftColumn->setVisibility(brls::Visibility::GONE);
    recyclingGrid->setVisibility(brls::Visibility::GONE);
    searchField->setVisibility(brls::Visibility::GONE);
    refreshButton->setVisibility(brls::Visibility::GONE);
    backButton->setVisibility(brls::Visibility::GONE);
    contentHub->setVisibility(brls::Visibility::VISIBLE);

    brls::Application::giveFocus(hubLive);
}

void HomeLive::enterContentType(int contentType) {
    brls::Logger::info("HomeLive: entering Xtream content type {}", contentType);
    inHubMode        = false;
    inSeriesEpisodes = false;
    ProgramConfig::instance().setXtreamContentType(contentType);

    // The series handler is only active while the series list is visible
    if (contentType == 2) {
        g_xtreamSeriesHandler = [this](const tsvitch::LiveM3u8& series) { this->openSeriesEpisodes(series); };
    } else {
        g_xtreamSeriesHandler = nullptr;
    }

    currentLoadType = contentType;

    std::string labelKey = contentType == 2   ? "tsvitch/xtream/content/series"
                           : contentType == 1 ? "tsvitch/xtream/content/movies"
                                              : "tsvitch/xtream/content/live";
    backLabel->setText(brls::getStr(labelKey));
    updateActionLabels();

    contentHub->setVisibility(brls::Visibility::GONE);
    leftColumn->setVisibility(brls::Visibility::VISIBLE);
    recyclingGrid->setVisibility(brls::Visibility::VISIBLE);
    searchField->setVisibility(brls::Visibility::VISIBLE);
    refreshButton->setVisibility(brls::Visibility::VISIBLE);
    backButton->setVisibility(brls::Visibility::VISIBLE);

    ProgramConfig::instance().setSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);
    {
        std::lock_guard<std::mutex> lock(groupCacheMutex);
        groupCache.clear();
    }
    channelsList.clear();
    isSearchActive     = false;
    selectedGroupIndex = 0;

    // Cache hit: renderiza sem refazer o fetch
    auto cached = contentCache.find(contentType);
    if (cached != contentCache.end() && !cached->second.empty()) {
        brls::Logger::info("HomeLive: content type {} served from cache ({} items)", contentType, cached->second.size());
        this->onLiveList(cached->second, false);
        brls::Application::giveFocus(recyclingGrid);
        return;
    }

    recyclingGrid->showSkeleton();
    upRecyclingGrid->setVisibility(brls::Visibility::GONE);
    this->requestLiveList();
    brls::Application::giveFocus(recyclingGrid);
}

void HomeLive::updateActionLabels() {
    if (!isXtreamMode) return;
    const char* nounKey = currentLoadType == 2   ? "tsvitch/xtream/noun/series"
                          : currentLoadType == 1 ? "tsvitch/xtream/noun/movies"
                                                 : "tsvitch/xtream/noun/live";
    std::string noun = brls::getStr(nounKey);
    searchLabel->setText(brls::getStr("tsvitch/xtream/action/search") + " " + noun);
    refreshLabel->setText(brls::getStr("tsvitch/xtream/action/refresh") + " " + noun);
}

void HomeLive::refreshCurrent() {
    brls::Logger::info("HomeLive: refresh requested (episodes={}, type={})", inSeriesEpisodes, currentLoadType);
    isSearchActive = false;
    {
        std::lock_guard<std::mutex> lock(groupCacheMutex);
        groupCache.clear();
    }
    recyclingGrid->showSkeleton();
    upRecyclingGrid->setVisibility(brls::Visibility::GONE);

    if (inSeriesEpisodes) {
        // Recarrega os episódios da série atual
        episodesCache.erase(currentSeriesId);
        tsvitch::LiveM3u8 series;
        series.id    = currentSeriesId;
        series.title = currentSeriesTitle;
        this->openSeriesEpisodes(series);
        return;
    }

    if (isXtreamMode) contentCache.erase(currentLoadType);
    channelsList.clear();
    ChannelManager::get()->remove();
    this->requestLiveList();
}

void HomeLive::openSeriesEpisodes(const tsvitch::LiveM3u8& series) {
    brls::Logger::info("HomeLive: opening episodes for series '{}' (id={})", series.title, series.id);

    // In the episodes view items are playable: disable the series handler
    g_xtreamSeriesHandler = nullptr;
    isSearchActive        = false;
    selectedGroupIndex    = 0;
    currentSeriesId       = series.id;
    currentSeriesTitle    = series.title;
    // Marca já como "em episódios" para que Voltar durante o carregamento
    // retorne à lista de séries (e não ao hub)
    inSeriesEpisodes      = true;
    {
        std::lock_guard<std::mutex> lock(groupCacheMutex);
        groupCache.clear();
    }

    // Cache hit: episódios já carregados desta série
    auto cachedEps = episodesCache.find(series.id);
    if (cachedEps != episodesCache.end() && !cachedEps->second.empty()) {
        brls::Logger::info("HomeLive: episodes for series {} served from cache ({} items)", series.id, cachedEps->second.size());
        inSeriesEpisodes = true;
        backLabel->setText(series.title);
        this->onLiveList(cachedEps->second, false);
        return;
    }

    recyclingGrid->showSkeleton();
    upRecyclingGrid->setVisibility(brls::Visibility::GONE);

    std::string seriesTitle = series.title;
    std::string seriesId    = series.id;
    auto isValid            = validityFlag;
    CLIENT::get_xtream_series_info(
        seriesId,
        [this, seriesTitle, seriesId, isValid](tsvitch::LiveM3u8ListResult episodes) {
            if (!isValid || !isValid->load()) return;
            // Se o usuário já voltou/trocou de série, ignora este resultado tardio
            if (!inSeriesEpisodes || currentSeriesId != seriesId) return;
            backLabel->setText(seriesTitle);
            if (episodes.empty()) {
                recyclingGrid->setEmpty();
                upRecyclingGrid->setVisibility(brls::Visibility::GONE);
                return;
            }
            // Reuse the group pipeline: seasons become the sidebar groups
            this->onLiveList(std::move(episodes), false);
        },
        [this, seriesId, isValid](const std::string& error, int) {
            if (!isValid || !isValid->load()) return;
            if (!inSeriesEpisodes || currentSeriesId != seriesId) return;
            this->onError(error);
        });
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

void HomeLive::onLiveList(tsvitch::LiveM3u8ListResult result, bool firstLoad) {
    brls::Logger::info("Fragment HomeLive: onLiveList - received {} channels", result.size());
    if (result.empty()) {
        recyclingGrid->setEmpty();
        upRecyclingGrid->setVisibility(brls::Visibility::GONE);
        return;
    }

    this->registerAction("hints/back"_i18n, brls::BUTTON_B, [this](...) {
        if (isSearchActive) {
            this->cancelSearch();
        } else if (inSeriesEpisodes) {
            // From the episodes, back returns to the series list
            this->enterContentType(2);
        } else if (isXtreamMode && !inHubMode) {
            // In Xtream, back returns to the 3-card hub instead of quitting the app
            this->showContentHub();
        } else {
            // Se houver download em andamento, avisa que sair irá cancelá-lo
            auto downloads = DownloadManager::instance().getAllDownloads();
            bool hasActive = false;
            for (const auto& d : downloads) {
                if (d.status == DownloadStatus::DOWNLOADING || d.status == DownloadStatus::PENDING ||
                    d.status == DownloadStatus::PAUSED) {
                    hasActive = true;
                    break;
                }
            }

            if (hasActive) {
                auto dialog = new brls::Dialog("tsvitch/download/exit_warning"_i18n);
                dialog->addButton("hints/cancel"_i18n, []() {});
                dialog->addButton("hints/ok"_i18n, [downloads]() {
                    // Cancela os downloads ativos antes de sair (evita travar/erro no fechamento)
                    for (const auto& d : downloads) {
                        if (d.status != DownloadStatus::COMPLETED) {
                            DownloadManager::instance().cancelDownload(d.id);
                        }
                    }
                    brls::Application::quit();
                });
                dialog->open();
            } else {
                auto dialog = new brls::Dialog("hints/exit_hint"_i18n);
                dialog->addButton("hints/cancel"_i18n, []() {});
                dialog->addButton("hints/ok"_i18n, []() { brls::Application::quit(); });
                dialog->open();
            }
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

    this->registerAction("tsvitch/download/action"_i18n, brls::BUTTON_RT, [this](...) {
        this->downloadVideo();
        return true;
    });

    // Salva channelsList SUBITO per accesso thread-safe
    this->channelsList = std::move(result); // Move invece di copy!

    // Guarda em cache (em memória) para não refazer o fetch ao voltar (só no Xtream)
    if (isXtreamMode) {
        if (inSeriesEpisodes && !currentSeriesId.empty()) {
            episodesCache[currentSeriesId] = this->channelsList;
        } else if (!inSeriesEpisodes) {
            contentCache[currentLoadType] = this->channelsList;
        }
    }

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

        // Memoriza os nomes das categorias (para a tela de controle parental)
        if (isXtreamMode && !inSeriesEpisodes) {
            ProgramConfig::instance().addKnownCategories(groupTitles);
        }

        // Leggi lastIndex dal config
        int lastIndex = ProgramConfig::instance().getSettingItem(SettingItem::GROUP_SELECTED_INDEX, 0);
        if (lastIndex >= (int)groupTitles.size()) lastIndex = 0;
        // Não abrir automaticamente uma categoria bloqueada: escolhe a primeira liberada
        if (!groupTitles.empty() && ProgramConfig::instance().isCategoryLocked(groupTitles[lastIndex]) &&
            !unlockedCategories.count(groupTitles[lastIndex])) {
            for (size_t i = 0; i < groupTitles.size(); ++i) {
                if (!ProgramConfig::instance().isCategoryLocked(groupTitles[i]) ||
                    unlockedCategories.count(groupTitles[i])) {
                    lastIndex = (int)i;
                    break;
                }
            }
        }
        std::string selectedGroup = groupTitles.empty() ? "" : groupTitles[lastIndex];
        
        // Prepara il gruppo selezionato (não revela se estiver bloqueado)
        bool selectedLocked = ProgramConfig::instance().isCategoryLocked(selectedGroup) &&
                              !unlockedCategories.count(selectedGroup);
        tsvitch::LiveM3u8ListResult filtered;
        if (!selectedLocked && !selectedGroup.empty() && groupIndices.count(selectedGroup)) {
            const auto& indices = groupIndices[selectedGroup];
            filtered.reserve(indices.size());
            for (size_t idx : indices) {
                filtered.push_back(this->channelsList[idx]);
            }
        }
        
        brls::Logger::info("HomeLive: Selected group '{}' with {} channels", selectedGroup, filtered.size());
        
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
                this->selectGroupContent(group);
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
    auto* datasource = dynamic_cast<DataSourceUpList*>(upRecyclingGrid->getDataSource());
    if (!datasource) return;
    if (index >= datasource->getItemCount()) return;
    this->selectedGroupIndex = index;
    // setSelectedIndex dispara onGroupSelected -> selectGroupContent (que aplica o bloqueio)
    datasource->setSelectedIndex(upRecyclingGrid, index);
    upRecyclingGrid->selectRowAt(index, false);

    brls::Logger::debug("selectGroupIndex: {}", index);
}

void HomeLive::selectGroupContent(const std::string& group) {
    // Bloqueio parental: categoria travada e não liberada nesta sessão pede o PIN
    if (ProgramConfig::instance().isCategoryLocked(group) && !unlockedCategories.count(group)) {
        recyclingGrid->setEmpty();
        this->promptCategoryPin(group, [this, group]() {
            unlockedCategories.insert(group);
            this->selectGroupContent(group);
        });
        return;
    }

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
}

void HomeLive::promptCategoryPin(const std::string& category, std::function<void()> onUnlock) {
    brls::Application::getImeManager()->openForText(
        [this, category, onUnlock](const std::string& text) {
            if (text == ProgramConfig::instance().getParentalPin()) {
                onUnlock();
            } else {
                brls::Application::notify("tsvitch/parental/wrong_pin"_i18n);
            }
        },
        "tsvitch/parental/enter_pin"_i18n, "", 8, "", 0);
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

    // If the 3-card hub is visible, load nothing until the user chooses
    if (inHubMode) {
        brls::Logger::debug("HomeLive onShow: hub visible, skipping load");
        return;
    }

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
    // Ottieni l'item attualmente focalizzato
    auto* item = dynamic_cast<RecyclingGridItemLiveVideoCard*>(this->recyclingGrid->getFocusedItem());
    if (!item) {
        brls::Logger::warning("HomeLive::downloadVideo: No focused item");
        return;
    }

    // Ottieni il canale
    tsvitch::LiveM3u8 channel = item->getChannel();
    
    // Controlla se è una live stream in corso
    if (tsvitch::isLiveStream(channel.url, channel.title)) {
        brls::Logger::warning("HomeLive: Cannot download live streams");
        tsvitch::showLiveStreamDownloadError();
        return;
    }
    
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
            
            brls::sync([id, filePath]() {
                // Nascondi l'overlay
                tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                
                // Non mostrare notifica se è un download già completato (duplicato)
                if (filePath != "Already completed") {
                    brls::Application::notify("tsvitch/download/completed"_i18n);
                } else {
                    brls::Application::notify("tsvitch/download/already"_i18n);
                }
            });
        },
        [](const std::string& id, const std::string& error) {
            // Callback di errore
            brls::Logger::error("Download {} failed: {}", id, error);
            brls::sync([id, error]() {
                // Nascondi l'overlay
                tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                brls::Application::notify("tsvitch/download/error"_i18n + std::string(": ") + error);
            });
        }
    );
    
    if (!downloadId.empty()) {
        // Controlla lo stato del download per vedere se è già completato
        auto downloadItem = DownloadManager::instance().getDownload(downloadId);
        
        if (downloadItem.status == DownloadStatus::COMPLETED) {
            // È un download già completato, non mostrare overlay
            brls::Logger::info("HomeLive: Skipped showing overlay for already completed download {} ({})", downloadId, channel.title);
        } else {
            // È un nuovo download o uno in corso, mostra l'overlay
            tsvitch::DownloadProgressManager::getInstance()->showDownloadProgress(
                downloadId, channel.title, channel.url
            );
            
            brls::Application::notify("tsvitch/download/started"_i18n + std::string(": ") + channel.title);
            brls::Logger::info("HomeLive: Started download {} for {}", downloadId, channel.title);
        }
    } else {
        brls::Application::notify("tsvitch/download/start_error"_i18n);
        brls::Logger::error("HomeLive: Failed to start download for {}", channel.title);
    }
}