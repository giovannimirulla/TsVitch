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
    std::string app_version = APPVersion::instance().git_tag;

    // Check IPTV mode and use appropriate parameters
    int iptvMode = ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE);
    nlohmann::json json_body = {
        {"language", lang}, 
        {"platform", platform}, 
        {"app_version", app_version}
    };

    if (iptvMode == 0) {
        // M3U8 Mode
        std::string m3u8_url = ProgramConfig::instance().getM3U8Url();
        json_body["m3u8_url"] = m3u8_url;
        brls::Logger::debug("Registering user with M3U8 mode - language: {}, platform: {}, app_version: {}, m3u8_url: {}", 
                            lang, platform, app_version, m3u8_url);
    } else if (iptvMode == 1) {
        // Xtream Codes Mode
        std::string server_url = ProgramConfig::instance().getXtreamServerUrl();
        std::string username = ProgramConfig::instance().getXtreamUsername();
        std::string password = ProgramConfig::instance().getXtreamPassword();
        json_body["xtream_server"] = server_url;
        json_body["xtream_username"] = username;
        json_body["xtream_password"] = password;
        brls::Logger::debug("Registering user with Xtream mode - language: {}, platform: {}, app_version: {}, server: {}, username: {}", 
                            lang, platform, app_version, server_url, username);
    }

    std::string json_str = json_body.dump();
    cpr::Body body{json_str};

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
                                         {"app_version", APPVersion::instance().git_tag}});
                    if (callback) {
                        callback(user_id, r.status_code);
                    }
                } else if (json_result.contains("error")) {
                    std::string err_msg = json_result["error"].get<std::string>();
                    GA("register_user", {{"error", err_msg}, {"status_code", std::to_string(r.status_code)}});
                    if (error) {
                        error(err_msg, r.status_code);
                    } else {
                        ERROR_MSG(err_msg, r.status_code);
                    }
                } else {
                    GA("register_user",
                       {{"error", "Unknown response"}, {"status_code", std::to_string(r.status_code)}});
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
    std::string app_version = APPVersion::instance().git_tag;

    // Check IPTV mode and use appropriate parameters
    int iptvMode = ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE);
    auto url = std::string(SERVER_URL_VALUE) + "/functions/v1/check-user";

    brls::Logger::debug("Getting ad with URL: {}", url);

    nlohmann::json json_body = {
        {"user_id", user_id},
        {"language", lang},
        {"platform", platform},
        {"app_version", app_version}
    };

    if (iptvMode == 0) {
        // M3U8 Mode
        std::string m3u8_url = ProgramConfig::instance().getM3U8Url();
        json_body["m3u8_url"] = m3u8_url;
        brls::Logger::debug("Checking user ID with M3U8 mode - user_id: {}, language: {}, platform: {}, app_version: {}, m3u8_url: {}",
                            user_id, lang, platform, app_version, m3u8_url);
    } else if (iptvMode == 1) {
        // Xtream Codes Mode
        std::string server_url = ProgramConfig::instance().getXtreamServerUrl();
        std::string username = ProgramConfig::instance().getXtreamUsername();
        std::string password = ProgramConfig::instance().getXtreamPassword();
        json_body["xtream_server"] = server_url;
        json_body["xtream_username"] = username;
        json_body["xtream_password"] = password;
        brls::Logger::debug("Checking user ID with Xtream mode - user_id: {}, language: {}, platform: {}, app_version: {}, server: {}, username: {}",
                            user_id, lang, platform, app_version, server_url, username);
    }

    std::string json_str = json_body.dump();
    cpr::Body body{json_str};

    HTTP::__cpr_post(
        url, cpr::Parameters{}, body,

        [callback, error](const cpr::Response& r) {
            try {
                auto json_result = nlohmann::json::parse(r.text);

                brls::Logger::debug("Check user ID response: {}", json_result.dump());
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
                    GA("check_user_id",
                       {{"error", "Unknown response"}, {"status_code", std::to_string(r.status_code)}});
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
    std::string app_version = APPVersion::instance().git_tag;

    // Check IPTV mode and use appropriate parameters
    int iptvMode = ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE);
    auto url = std::string(SERVER_URL_VALUE) + "/functions/v1/get-ad";

    brls::Logger::debug("Getting ad with URL: {}", url);

    nlohmann::json json_body = {
        {"user_id", user_id},
        {"language", lang},
        {"platform", platform},
        {"app_version", app_version}
    };

    if (iptvMode == 0) {
        // M3U8 Mode
        std::string m3u8_url = ProgramConfig::instance().getM3U8Url();
        json_body["m3u8_url"] = m3u8_url;
        brls::Logger::debug("Getting ad with M3U8 mode - user_id: {}, language: {}, platform: {}, app_version: {}, m3u8_url: {}",
                            user_id, lang, platform, app_version, m3u8_url);
    } else if (iptvMode == 1) {
        // Xtream Codes Mode
        std::string server_url = ProgramConfig::instance().getXtreamServerUrl();
        std::string username = ProgramConfig::instance().getXtreamUsername();
        std::string password = ProgramConfig::instance().getXtreamPassword();
        json_body["xtream_server"] = server_url;
        json_body["xtream_username"] = username;
        json_body["xtream_password"] = password;
        brls::Logger::debug("Getting ad with Xtream mode - user_id: {}, language: {}, platform: {}, app_version: {}, server: {}, username: {}",
                            user_id, lang, platform, app_version, server_url, username);
    }

    std::string json_str = json_body.dump();
    cpr::Body body{json_str};

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