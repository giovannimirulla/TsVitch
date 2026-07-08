#pragma once

#include "presenter/home_vod.hpp"
#include "view/auto_tab_frame.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include "utils/xtream_helper.hpp"
#include <map>
#include <mutex>
#include <memory>

class RecyclingGrid;
namespace brls { class View; }

class HomeVOD : public AttachedView, public HomeVODRequest {
public:
    HomeVOD();
    ~HomeVOD() override;

    void onCreate() override;
    void onShow() override;

    void onVODCategories(const std::vector<tsvitch::XtreamVODCategory>& categories) override;
    void onVODStreams(const std::vector<tsvitch::XtreamVODStream>& streams, const std::string& categoryName) override;
    void onError(const std::string& error) override;

    static brls::View* create();

private:
    void selectCategory(size_t index);
    void buildCategoryList(const std::vector<tsvitch::XtreamVODCategory>& categories);
    void buildStreams(const std::vector<tsvitch::XtreamVODStream>& streams, const std::string& categoryName);

    std::vector<tsvitch::XtreamVODCategory> vodCategories;
    std::vector<tsvitch::XtreamVODStream> vodStreams;
    tsvitch::LiveM3u8ListResult currentLiveList;
    size_t selectedCategoryIndex = 0;
    std::unique_ptr<class VodCategoryDataSource> categoryDataSource;
    std::unique_ptr<class VodStreamDataSource> streamDataSource;

    BRLS_BIND(RecyclingGrid, categoryGrid, "vod/categories");
    BRLS_BIND(RecyclingGrid, streamGrid, "vod/streams");
};
