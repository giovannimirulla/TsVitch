#include "fragment/home_series.hpp"

#include <borealis/core/logger.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/core/thread.hpp>

#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include "utils/xtream_helper.hpp"
#include <algorithm>

using namespace brls::literals;

class SeriesCategoryCell : public RecyclingGridItem {
public:
    SeriesCategoryCell() {
        this->inflateFromXMLRes("xml/views/group_channel_dynamic.xml");
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

    static RecyclingGridItem* create() { return new SeriesCategoryCell(); }

private:
    BRLS_BIND(brls::Label, labelTitle, "title");
    NVGcolor selectedColor{};
    NVGcolor fontColor{};
};

class SeriesCategoryDataSource : public RecyclingGridDataSource {
public:
    using OnCategorySelected = std::function<void(size_t)>;

    SeriesCategoryDataSource(std::vector<std::string> names, OnCategorySelected cb)
        : list(std::move(names)), onCategorySelected(std::move(cb)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = static_cast<SeriesCategoryCell*>(recycler->dequeueReusableCell("Cell"));
        item->setTitle(this->list[index]);
        item->setSelected(index == selectedIndex);
        return item;
    }

    size_t getItemCount() override { return list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= list.size()) return;

        std::vector<RecyclingGridItem*>& items = recycler->getGridItems();
        for (auto& i : items) {
            auto* cell = dynamic_cast<SeriesCategoryCell*>(i);
            if (cell) cell->setSelected(false);
        }

        selectedIndex = index;
        auto* item    = dynamic_cast<SeriesCategoryCell*>(recycler->getGridItemByIndex(index));
        if (item) item->setSelected(true);

        if (onCategorySelected) onCategorySelected(index);
    }

    void clearData() override { list.clear(); }

    void setSelectedIndex(RecyclingGrid* recycler, size_t index) {
        if (index >= list.size()) return;
        selectedIndex = index;
        auto* item    = dynamic_cast<SeriesCategoryCell*>(recycler->getGridItemByIndex(index));
        if (item) item->setSelected(true);
    }

private:
    std::vector<std::string> list;
    size_t selectedIndex = 0;
    OnCategorySelected onCategorySelected;
};

class SeriesDataSource : public RecyclingGridDataSource {
public:
    using OnSeriesSelected = std::function<void(size_t)>;

    SeriesDataSource(tsvitch::LiveM3u8ListResult list, OnSeriesSelected cb)
        : seriesList(std::move(list)), onSeriesSelected(std::move(cb)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(seriesList[index]);
        return item;
    }

    size_t getItemCount() override { return seriesList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= seriesList.size()) return;
        if (onSeriesSelected) onSeriesSelected(index);
    }

    void clearData() override { seriesList.clear(); }

private:
    tsvitch::LiveM3u8ListResult seriesList;
    OnSeriesSelected onSeriesSelected;
};

class EpisodeDataSource : public RecyclingGridDataSource {
public:
    using OnEpisodeSelected = std::function<void(size_t)>;

    EpisodeDataSource(tsvitch::LiveM3u8ListResult list, OnEpisodeSelected cb)
        : episodes(std::move(list)), onEpisodeSelected(std::move(cb)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(episodes[index]);
        return item;
    }

    size_t getItemCount() override { return episodes.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= episodes.size()) return;
        if (onEpisodeSelected) onEpisodeSelected(index);
    }

    void clearData() override { episodes.clear(); }

private:
    tsvitch::LiveM3u8ListResult episodes;
    OnEpisodeSelected onEpisodeSelected;
};

HomeSeries::HomeSeries() {
    this->inflateFromXMLRes("xml/fragment/home_series.xml");

    categoryGrid->registerCell("Cell", []() { return SeriesCategoryCell::create(); });
    seriesGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });
    episodeGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });
}

HomeSeries::~HomeSeries() { brls::Logger::debug("HomeSeries destroyed"); }

void HomeSeries::onCreate() {
    brls::Logger::info("HomeSeries::onCreate");
    categoryGrid->showSkeleton();
    seriesGrid->showSkeleton();
    episodeGrid->showSkeleton();
    requestSeriesCategories();
}

void HomeSeries::onShow() {
    brls::Logger::debug("HomeSeries::onShow");
}

void HomeSeries::onSeriesCategories(const std::vector<tsvitch::XtreamSeriesCategory>& categories) {
    seriesCategories = categories;
    buildCategoryList(categories);
}

void HomeSeries::onSeriesList(const std::vector<tsvitch::XtreamSeries>& list, const std::string& categoryName) {
    seriesList = list;
    buildSeries(list, categoryName);
}

void HomeSeries::onSeriesInfo(const tsvitch::XtreamSeriesInfo& info) {
    currentSeriesName = info.name;
    currentSeriesId = info.series_id;
    buildEpisodes(info);
    showInfoDialog(info);
}

void HomeSeries::onError(const std::string& error) {
    brls::Logger::error("HomeSeries error: {}", error);
    auto* dialog = new brls::Dialog(error);
    dialog->addButton("OK", []() {});
    dialog->open();
}

void HomeSeries::buildCategoryList(const std::vector<tsvitch::XtreamSeriesCategory>& categories) {
    std::vector<std::string> names;
    names.reserve(categories.size());
    for (const auto& c : categories) names.push_back(c.category_name);

    categoryDataSource = std::make_unique<SeriesCategoryDataSource>(names, [this](size_t index) { selectCategory(index); });
    categoryGrid->setDataSource(categoryDataSource.get());

    if (!categories.empty()) {
        categoryDataSource->setSelectedIndex(categoryGrid, 0);
        selectCategory(0);
    }
}

void HomeSeries::selectCategory(size_t index) {
    if (index >= seriesCategories.size()) return;
    const auto& cat = seriesCategories[index];
    seriesGrid->showSkeleton();
    requestSeriesByCategory(cat.category_id, cat.category_name);
}

void HomeSeries::buildSeries(const std::vector<tsvitch::XtreamSeries>& series, const std::string& categoryName) {
    tsvitch::LiveM3u8ListResult mapped;
    mapped.reserve(series.size());
    for (const auto& s : series) {
        tsvitch::LiveM3u8 item;
        item.id         = s.series_id;
        item.chno       = std::to_string(s.num);
        item.title      = s.name;
        item.logo       = s.cover;
        item.groupTitle = categoryName;
        item.url        = ""; // non abbiamo stream episode qui
        mapped.push_back(item);
    }

    seriesDataSource = std::make_unique<SeriesDataSource>(mapped, [this](size_t idx) {
        if (idx >= seriesList.size()) return;
        if (episodeGrid) episodeGrid->showSkeleton();
        requestSeriesInfo(seriesList[idx].series_id);
    });
    seriesGrid->setDataSource(seriesDataSource.get());
}

void HomeSeries::showInfoDialog(const tsvitch::XtreamSeriesInfo& info) {
    std::string body;
    body += info.plot + "\n\n";
    if (!info.genre.empty()) body += "Genere: " + info.genre + "\n";
    if (!info.cast.empty()) body += "Cast: " + info.cast + "\n";
    if (!info.director.empty()) body += "Regia: " + info.director + "\n";
    if (!info.rating.empty()) body += "Rating: " + info.rating + " (" + info.rating_5based + "/5)\n";
    if (!info.releaseDate.empty()) body += "Data: " + info.releaseDate + "\n";
    if (!info.episode_run_time.empty()) body += "Durata episodio: " + info.episode_run_time + " min\n";

    auto* dialog = new brls::Dialog(info.name + "\n\n" + body);
    dialog->addButton("OK", []() {});
    dialog->open();
}

void HomeSeries::buildEpisodes(const tsvitch::XtreamSeriesInfo& info) {
    episodeMapped.clear();
    episodes.clear();
    struct EpisodeView {
        tsvitch::XtreamEpisode ep;
        std::string seasonTag;
    };

    std::vector<EpisodeView> flat;
    for (const auto& season : info.seasons) {
        for (const auto& ep : season.episodes) {
            EpisodeView v{ep, season.season_number.empty() ? ep.season : season.season_number};
            flat.push_back(std::move(v));
        }
    }

    auto toInt = [](const std::string& val) {
        try {
            return std::stoi(val);
        } catch (...) {
            return 0;
        }
    };

    std::sort(flat.begin(), flat.end(), [&](const EpisodeView& a, const EpisodeView& b) {
        int sa = toInt(a.seasonTag);
        int sb = toInt(b.seasonTag);
        if (sa == sb) return toInt(a.ep.episode_num) < toInt(b.ep.episode_num);
        return sa < sb;
    });

    for (const auto& view : flat) {
        const auto& ep = view.ep;
        episodes.push_back(ep);
        tsvitch::LiveM3u8 m;
        std::string seasonTag = view.seasonTag.empty() ? ep.season : view.seasonTag;
        std::string epNum = ep.episode_num;
        std::string title = "S" + seasonTag + "E" + epNum;
        if (!ep.title.empty()) title += " - " + ep.title;
        m.id = ep.id;
        m.chno = epNum;
        m.title = title;
        m.logo = info.cover;
        m.groupTitle = info.name;
        std::string ext = ep.container_extension.empty() ? "mkv" : ep.container_extension;

        std::string url;
        if (!ep.direct_source.empty()) {
            url = ep.direct_source;
        } else if (!ep.id.empty()) {
            url = tsvitch::XtreamAPI::instance().getSeriesEpisodeUrl(ep.id, ext);
        } else {
            url = tsvitch::XtreamAPI::instance().getSeriesUrl(info.series_id, seasonTag, epNum, ext);
        }
        m.url = url;
        episodeMapped.push_back(m);
    }

    episodeDataSource = std::make_unique<EpisodeDataSource>(episodeMapped, [this](size_t idx) {
        if (idx >= episodeMapped.size()) return;
        Intent::openLive(episodeMapped, idx, [this]() {
            if (episodeGrid) episodeGrid->reloadData();
        });
    });
    episodeGrid->setDataSource(episodeDataSource.get());
}

brls::View* HomeSeries::create() { return new HomeSeries(); }
