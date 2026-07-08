#include "presenter/home_series.hpp"
#include <borealis/core/logger.hpp>

using namespace tsvitch;

HomeSeriesRequest::~HomeSeriesRequest() {
    if (validityFlag) validityFlag->store(false);
}

bool HomeSeriesRequest::ensureXtreamConfigured(std::string& server, std::string& user, std::string& pass) {
    auto& cfg = ProgramConfig::instance();
    int iptvMode  = cfg.getSettingItem(SettingItem::IPTV_MODE, 0);
    bool enabled  = cfg.getXtreamEnabled();
    if (iptvMode == 1 && !enabled) {
        cfg.setXtreamEnabled(true);
        enabled = true;
    }

    if (!enabled) {
        onError("Xtream disabilitato nelle impostazioni");
        return false;
    }

    server = cfg.getXtreamServerUrl();
    user = cfg.getXtreamUsername();
    pass = cfg.getXtreamPassword();

    if (server.empty() || user.empty() || pass.empty()) {
        onError("Credenziali Xtream mancanti");
        return false;
    }

    XtreamAPI::instance().setCredentials(server, user, pass);
    return true;
}

void HomeSeriesRequest::requestSeriesCategories() {
    if (isCategoriesInProgress.exchange(true)) {
        brls::Logger::debug("HomeSeriesRequest: categories request already in progress");
        return;
    }

    std::string server, user, pass;
    if (!ensureXtreamConfigured(server, user, pass)) {
        isCategoriesInProgress = false;
        return;
    }

    auto flag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = flag;

    brls::Logger::info("HomeSeriesRequest: fetching series categories");
    XtreamAPI::instance().getSeriesCategories([
        this, flag
    ](const std::vector<XtreamSeriesCategory>& categories, bool success, const std::string& error) {
        if (!flag->load()) {
            brls::Logger::debug("HomeSeriesRequest: categories callback after destruction");
            isCategoriesInProgress = false;
            return;
        }

        if (!success) {
            brls::Logger::error("HomeSeriesRequest: categories error: {}", error);
            onError(error);
            isCategoriesInProgress = false;
            return;
        }

        try {
            onSeriesCategories(categories);
        } catch (...) {
            brls::Logger::error("HomeSeriesRequest: exception in onSeriesCategories");
        }
        isCategoriesInProgress = false;
    });
}

void HomeSeriesRequest::requestSeriesByCategory(const std::string& categoryId, const std::string& categoryName) {
    if (isSeriesInProgress.exchange(true)) {
        brls::Logger::debug("HomeSeriesRequest: series request already in progress");
        return;
    }

    std::string server, user, pass;
    if (!ensureXtreamConfigured(server, user, pass)) {
        isSeriesInProgress = false;
        return;
    }

    auto flag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = flag;

    brls::Logger::info("HomeSeriesRequest: fetching series for category {}", categoryId);
    XtreamAPI::instance().getSeriesByCategory(categoryId, [
        this, flag, categoryName
    ](const std::vector<XtreamSeries>& list, bool success, const std::string& error) {
        if (!flag->load()) {
            brls::Logger::debug("HomeSeriesRequest: series callback after destruction");
            isSeriesInProgress = false;
            return;
        }

        if (!success) {
            brls::Logger::error("HomeSeriesRequest: series error: {}", error);
            onError(error);
            isSeriesInProgress = false;
            return;
        }

        try {
            onSeriesList(list, categoryName);
        } catch (...) {
            brls::Logger::error("HomeSeriesRequest: exception in onSeriesList");
        }
        isSeriesInProgress = false;
    });
}

void HomeSeriesRequest::requestSeriesInfo(const std::string& seriesId) {
    if (isInfoInProgress.exchange(true)) {
        brls::Logger::debug("HomeSeriesRequest: series info request already in progress");
        return;
    }

    std::string server, user, pass;
    if (!ensureXtreamConfigured(server, user, pass)) {
        isInfoInProgress = false;
        return;
    }

    auto flag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = flag;

    brls::Logger::info("HomeSeriesRequest: fetching series info {}", seriesId);
    XtreamAPI::instance().getSeriesInfo(seriesId, [
        this, flag
    ](const XtreamSeriesInfo& info, bool success, const std::string& error) {
        if (!flag->load()) {
            brls::Logger::debug("HomeSeriesRequest: series info callback after destruction");
            isInfoInProgress = false;
            return;
        }

        if (!success) {
            brls::Logger::error("HomeSeriesRequest: series info error: {}", error);
            onError(error);
            isInfoInProgress = false;
            return;
        }

        try {
            onSeriesInfo(info);
        } catch (...) {
            brls::Logger::error("HomeSeriesRequest: exception in onSeriesInfo");
        }
        isInfoInProgress = false;
    });
}
