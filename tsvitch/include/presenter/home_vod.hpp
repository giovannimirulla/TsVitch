#pragma once

#include "presenter/presenter.h"
#include "utils/xtream_helper.hpp"
#include "utils/config_helper.hpp"
#include <atomic>
#include <memory>

class HomeVODRequest : public Presenter {
public:
    virtual void onVODCategories(const std::vector<tsvitch::XtreamVODCategory>& categories) = 0;
    virtual void onVODStreams(const std::vector<tsvitch::XtreamVODStream>& streams, const std::string& categoryName) = 0;
    virtual void onError(const std::string& error) = 0;

    void requestVODCategories();
    void requestVODStreams(const std::string& categoryId, const std::string& categoryName);

    virtual ~HomeVODRequest();

protected:
    std::shared_ptr<std::atomic<bool>> validityFlag;
    std::atomic<bool> isCategoryRequestInProgress{false};
    std::atomic<bool> isStreamsRequestInProgress{false};

private:
    bool ensureXtreamConfigured(std::string& server, std::string& user, std::string& pass);
};
