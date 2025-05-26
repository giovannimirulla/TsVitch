#include "tsvitch.h"
#include "tsvitch/util/http.hpp"
#include <borealis/core/application.hpp>
#include <random>
#include <utility>

#include "utils/config_helper.hpp"

#include "config/server_config.h"

namespace tsvitch {

void TsVitchClient::register_user(const std::function<void(const std::string&, int)>& callback,
                                  const ErrorCallback& error) {
    std::string lang        = brls::Application::getLocale();
    std::string platform    = APPVersion::instance().getPlatform();
    std::string app_version = APPVersion::instance().git_tag.empty() ? "v" + APPVersion::instance().getVersionStr()
                                                                     : APPVersion::instance().git_tag;

    nlohmann::json json_body = {{"language", lang}, {"platform", platform}, {"app_version", app_version}};
    std::string json_str     = json_body.dump();
    cpr::Body body{json_str};

    brls::Logger::debug("Registering user with language: {}, platform: {}, app_version: {}", lang, platform,
                        app_version);

    auto url = std::string(SERVER_URL_VALUE) + "/functions/v1/register-user";
    brls::Logger::debug("Registering user with URL: {}", url);

    HTTP::__cpr_post(
        url, cpr::Parameters{}, body,

        [callback, error](const cpr::Response& r) {
            try {
                auto json_result = nlohmann::json::parse(r.text);
                brls::Logger::debug("Register user response: {}", json_result.dump());
        
                if (r.status_code == 200 && json_result.contains("user_id")) {
                    std::string user_id = json_result["user_id"].get<std::string>();
                    ProgramConfig::instance().setDeviceID(user_id);
                    GA("register_user", {{"user_id", user_id},
                                         {"language", brls::Application::getLocale()},
                                         {"platform", APPVersion::instance().getPlatform()},
                                         {"app_version", APPVersion::instance().git_tag.empty()
                                                            ? "v" + APPVersion::instance().getVersionStr()
                                                            : APPVersion::instance().git_tag}});
                    if (callback) {
                        callback(user_id, r.status_code);
                    }
                } else if (json_result.contains("error")) {
                    std::string err_msg = json_result["error"].get<std::string>();
                     GA("register_user", {{"error", err_msg},
                                         {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error(err_msg, r.status_code);
                    } else {
                        ERROR_MSG(err_msg, r.status_code);
                    }
                } else {
                    GA("register_user", {{"error", "Unknown response"},
                                         {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error("Unknown response", r.status_code);
                    } else {
                        ERROR_MSG("Unknown response", r.status_code);
                    }
                }
            } catch (const std::exception&) {
                ERROR_MSG("Failed to register user", r.status_code);
                // GA call for register_user exception
                GA("register_user", {{"status_code", std::to_string(r.status_code)}});
                if (error) {
                    error("Failed to register user", r.status_code);
                }
            }
        },
        error);
}

void TsVitchClient::check_user_id(const std::function<void(const std::string&, int)>& callback,
                                  const ErrorCallback& error) {
    std::string user_id = ProgramConfig::instance().getDeviceID();
    if (user_id.empty()) {
        // GA call for check_user_id error
        if (error) {
            error("User ID is empty", -1);
        } else {
            ERROR_MSG("User ID is empty", -1);
        }
        return;
    }
    auto url = std::string(SERVER_URL_VALUE) + "/functions/v1/check-user";

    brls::Logger::debug("Checking user ID with URL: {}", url);

    nlohmann::json json_body = {{"user_id", user_id}};
    std::string json_str     = json_body.dump();
    cpr::Body body{json_str};

    HTTP::__cpr_post(
        url, cpr::Parameters{}, body,

        [callback, error](const cpr::Response& r) {
            try {
                auto json_result = nlohmann::json::parse(r.text);
                // GA call for check_user_id response
                if (r.status_code == 200 && json_result.contains("exists")) {
                    bool exists = json_result["exists"].get<bool>();

                    if (callback) {
                        GA("check_user_id", {{"user_id", ProgramConfig::instance().getDeviceID()},
                                           {"exists", exists ? "true" : "false"}});
                        callback(exists ? "true" : "false", r.status_code);
                    }
                } else if (json_result.contains("error")) {
                    std::string err_msg = json_result["error"].get<std::string>();
                    GA("check_user_id", {{"error", err_msg}, {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error(err_msg, r.status_code);
                    } else {
                        ERROR_MSG(err_msg, r.status_code);
                    }
                } else {
                    GA("check_user_id", {{"error", "Unknown response"}, {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error("Unknown response", r.status_code);
                    } else {
                        ERROR_MSG("Unknown response", r.status_code);
                    }
                }
            } catch (const std::exception&) {
                ERROR_MSG("Failed to check user ID", r.status_code);
                // GA call for check_user_id exception
                GA("check_user_id", {{"status_code", std::to_string(r.status_code)}});
                if (error) {
                    error("Failed to check user ID", r.status_code);
                }
            }
        },
        error);
}

void TsVitchClient::get_ad(const std::function<void(const std::string&, int)>& callback, const ErrorCallback& error) {
    std::string user_id = ProgramConfig::instance().getDeviceID();
    if (user_id.empty()) {
        // GA call for get_ad error
        if (error) {
            error("User ID is empty", -1);
        } else {
            ERROR_MSG("User ID is empty", -1);
        }
        return;
    }
    std::string lang = brls::Application::getLocale();
    if (lang.empty()) {
        lang = "en";
    }
    std::string platform = APPVersion::instance().getPlatform();
    if (platform.empty()) {
        platform = "unknown";
    }
    std::string app_version = APPVersion::instance().git_tag.empty() ? "v" + APPVersion::instance().getVersionStr()
                                                                     : APPVersion::instance().git_tag;
    auto url                = std::string(SERVER_URL_VALUE) + "/functions/v1/get-ad";

    brls::Logger::debug("Getting ad with URL: {}", url);

    nlohmann::json json_body = {
        {"user_id", user_id}, {"language", lang}, {"platform", platform}, {"app_version", app_version}};
    std::string json_str = json_body.dump();
    cpr::Body body{json_str};

    brls::Logger::debug("Getting ad with user_id: {}, language: {}, platform: {}, app_version: {}", user_id, lang,
                        platform, app_version);

    HTTP::__cpr_post(
        url, cpr::Parameters{}, body,

        [callback, error](const cpr::Response& r) {
            brls::Logger::debug("Get ad response: {}", r.text);
            try {
                auto json_result = nlohmann::json::parse(r.text);
                // GA call for get_ad response
                GA("get_ad", {{"status_code", std::to_string(r.status_code)}, {"response", r.text}});
                if (r.status_code == 200 && json_result.contains("ad") && json_result["ad"].contains("url")) {
                    std::string url = json_result["ad"]["url"].get<std::string>();
                    if (callback) {
                        callback(url, r.status_code);
                    }
                } else if (json_result.contains("error")) {
                    std::string err_msg = json_result["error"].get<std::string>();
                    GA("get_ad", {{"error", err_msg}, {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error(err_msg, r.status_code);
                    } else {
                        ERROR_MSG(err_msg, r.status_code);
                    }
                } else {
                    GA("get_ad", {{"error", "Unknown response"}, {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error("Unknown response", r.status_code);
                    } else {
                        ERROR_MSG("Unknown response", r.status_code);
                    }
                }
            } catch (const std::exception&) {
                ERROR_MSG("Failed to get ad", r.status_code);
                // GA call for get_ad exception
                GA("get_ad", {{"status_code", std::to_string(r.status_code)}});
                if (error) {
                    error("Failed to get ad", r.status_code);
                }
            }
        },
        error);
}
}  // namespace tsvitch