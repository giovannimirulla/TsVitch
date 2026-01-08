#pragma once

#include "presenter/presenter.h"
#include "utils/xtream_helper.hpp"
#include "utils/config_helper.hpp"
#include <atomic>
#include <memory>

class HomeSeriesRequest : public Presenter {
public:
    virtual void onSeriesCategories(const std::vector<tsvitch::XtreamSeriesCategory>& categories) = 0;
    virtual void onSeriesList(const std::vector<tsvitch::XtreamSeries>& series, const std::string& categoryName) = 0;
    virtual void onSeriesInfo(const tsvitch::XtreamSeriesInfo& info) = 0;
    virtual void onError(const std::string& error) = 0;

    void requestSeriesCategories();
    void requestSeriesByCategory(const std::string& categoryId, const std::string& categoryName);
    void requestSeriesInfo(const std::string& seriesId);

    virtual ~HomeSeriesRequest();

protected:
    std::shared_ptr<std::atomic<bool>> validityFlag;
    std::atomic<bool> isCategoriesInProgress{false};
    std::atomic<bool> isSeriesInProgress{false};
    std::atomic<bool> isInfoInProgress{false};

private:
    bool ensureXtreamConfigured(std::string& server, std::string& user, std::string& pass);
};
