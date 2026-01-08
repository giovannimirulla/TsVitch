#include "presenter/home_vod.hpp"
#include <borealis/core/logger.hpp>

using namespace tsvitch;

HomeVODRequest::~HomeVODRequest() {
    if (validityFlag) validityFlag->store(false);
}

bool HomeVODRequest::ensureXtreamConfigured(std::string& server, std::string& user, std::string& pass) {
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

void HomeVODRequest::requestVODCategories() {
    if (isCategoryRequestInProgress.exchange(true)) {
        brls::Logger::debug("HomeVODRequest: categories request already in progress");
        return;
    }

    std::string server, user, pass;
    if (!ensureXtreamConfigured(server, user, pass)) {
        isCategoryRequestInProgress = false;
        return;
    }

    auto flag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = flag;

    brls::Logger::info("HomeVODRequest: fetching VOD categories");
    XtreamAPI::instance().getVODCategories([
        this, flag
    ](const std::vector<XtreamVODCategory>& categories, bool success, const std::string& error) {
        if (!flag->load()) {
            brls::Logger::debug("HomeVODRequest: categories callback after destruction");
            isCategoryRequestInProgress = false;
            return;
        }

        if (!success) {
            brls::Logger::error("HomeVODRequest: categories error: {}", error);
            onError(error);
            isCategoryRequestInProgress = false;
            return;
        }

        try {
            onVODCategories(categories);
        } catch (...) {
            brls::Logger::error("HomeVODRequest: exception in onVODCategories handler");
        }
        isCategoryRequestInProgress = false;
    });
}

void HomeVODRequest::requestVODStreams(const std::string& categoryId, const std::string& categoryName) {
    if (isStreamsRequestInProgress.exchange(true)) {
        brls::Logger::debug("HomeVODRequest: streams request already in progress");
        return;
    }

    std::string server, user, pass;
    if (!ensureXtreamConfigured(server, user, pass)) {
        isStreamsRequestInProgress = false;
        return;
    }

    auto flag = std::make_shared<std::atomic<bool>>(true);
    validityFlag = flag;

    brls::Logger::info("HomeVODRequest: fetching VOD streams for category {}", categoryId);
    XtreamAPI::instance().getVODStreams(categoryId, [
        this, flag, categoryName
    ](const std::vector<XtreamVODStream>& streams, bool success, const std::string& error) {
        if (!flag->load()) {
            brls::Logger::debug("HomeVODRequest: streams callback after destruction");
            isStreamsRequestInProgress = false;
            return;
        }

        if (!success) {
            brls::Logger::error("HomeVODRequest: streams error: {}", error);
            onError(error);
            isStreamsRequestInProgress = false;
            return;
        }

        try {
            onVODStreams(streams, categoryName);
        } catch (...) {
            brls::Logger::error("HomeVODRequest: exception in onVODStreams handler");
        }
        isStreamsRequestInProgress = false;
    });
}
