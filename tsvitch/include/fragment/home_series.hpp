#pragma once

#include "presenter/home_series.hpp"
#include "view/auto_tab_frame.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include "utils/xtream_helper.hpp"
#include <memory>

class RecyclingGrid;
namespace brls { class View; }

class HomeSeries : public AttachedView, public HomeSeriesRequest {
public:
    HomeSeries();
    ~HomeSeries() override;

    void onCreate() override;
    void onShow() override;

    void onSeriesCategories(const std::vector<tsvitch::XtreamSeriesCategory>& categories) override;
    void onSeriesList(const std::vector<tsvitch::XtreamSeries>& series, const std::string& categoryName) override;
    void onSeriesInfo(const tsvitch::XtreamSeriesInfo& info) override;
    void onError(const std::string& error) override;

    static brls::View* create();

private:
    void selectCategory(size_t index);
    void buildCategoryList(const std::vector<tsvitch::XtreamSeriesCategory>& categories);
    void buildSeries(const std::vector<tsvitch::XtreamSeries>& series, const std::string& categoryName);
    void showInfoDialog(const tsvitch::XtreamSeriesInfo& info);
    void buildEpisodes(const tsvitch::XtreamSeriesInfo& info);

    std::vector<tsvitch::XtreamSeriesCategory> seriesCategories;
    std::vector<tsvitch::XtreamSeries> seriesList;
    std::vector<tsvitch::XtreamEpisode> episodes;
    tsvitch::LiveM3u8ListResult episodeMapped;
    std::string currentSeriesName;
     std::string currentSeriesId;

    std::unique_ptr<class SeriesCategoryDataSource> categoryDataSource;
    std::unique_ptr<class SeriesDataSource> seriesDataSource;
    std::unique_ptr<class EpisodeDataSource> episodeDataSource;

    BRLS_BIND(RecyclingGrid, categoryGrid, "series/categories");
    BRLS_BIND(RecyclingGrid, seriesGrid, "series/list");
    BRLS_BIND(RecyclingGrid, episodeGrid, "series/episodes");
};
