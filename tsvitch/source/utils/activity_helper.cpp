

#include <iostream>
#include <borealis/core/application.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/core/thread.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>
#include <pystring.h>

#include "activity/live_player_activity.hpp"
<<<<<<< HEAD
#include "activity/settings_activity.hpp"
=======

#include "activity/settings_activity.hpp"

>>>>>>> library-updates
#include "activity/main_activity.hpp"
#include "activity/hint_activity.hpp"

#include "utils/activity_helper.hpp"
#include "utils/config_helper.hpp"
#include "view/grid_dropdown.hpp"

static void showSeriesEpisodes(const tsvitch::LiveM3u8& seriesItem, std::function<void()> onClose) {
    // Show a loading dialog
    auto loadingDialog = new brls::Dialog("Loading episodes...");
    loadingDialog->open();
    
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username = ProgramConfig::instance().getXtreamUsername();
    auto password = ProgramConfig::instance().getXtreamPassword();
    
    if (serverUrl.empty()) {
        loadingDialog->close();
        auto dialog = new brls::Dialog("Xtream credentials not configured");
        dialog->addButton("OK", []() {});
        dialog->open();
        return;
    }
    
    if (serverUrl.back() != '/') serverUrl += "/";
    std::string seriesInfoUrl = serverUrl + "player_api.php?username=" + username + "&password=" + password + "&action=get_series_info&series_id=" + seriesItem.id;
    
    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    if (timeoutMs < 45000) timeoutMs = 45000;
    
    cpr::GetCallback([seriesItem, loadingDialog, serverUrl, username, password, onClose](const cpr::Response& r) {
        brls::sync([loadingDialog]() {
            loadingDialog->close();
        });
        
        if (r.status_code != 200 || r.text.empty()) {
            brls::sync([]() {
                auto dialog = new brls::Dialog("Failed to load series details");
                dialog->addButton("OK", []() {});
                dialog->open();
            });
            return;
        }
        
        try {
            auto jsonResult = nlohmann::json::parse(r.text);
            if (jsonResult.contains("episodes")) {
                const auto& episodesJson = jsonResult["episodes"];
                
                std::vector<std::string> seasonNames;
                std::vector<std::vector<tsvitch::LiveM3u8>> episodesBySeason;
                
                if (episodesJson.is_object()) {
                    std::vector<std::string> seasonKeys;
                    for (auto it = episodesJson.begin(); it != episodesJson.end(); ++it) {
                        seasonKeys.push_back(it.key());
                    }
                    std::sort(seasonKeys.begin(), seasonKeys.end(), [](const std::string& a, const std::string& b) {
                        try {
                            return std::stoi(a) < std::stoi(b);
                        } catch (...) {
                            return a < b;
                        }
                    });
                    
                    for (const auto& seasonNum : seasonKeys) {
                        const auto& seasonEpisodes = episodesJson[seasonNum];
                        if (seasonEpisodes.is_array()) {
                            std::vector<tsvitch::LiveM3u8> eps;
                            for (const auto& epItem : seasonEpisodes) {
                                tsvitch::LiveM3u8 ep;
                                if (epItem.contains("id") && !epItem["id"].is_null()) {
                                    if (epItem["id"].is_string()) ep.id = epItem["id"].get<std::string>();
                                    else if (epItem["id"].is_number()) ep.id = std::to_string(epItem["id"].get<int>());
                                }
                                
                                std::string epTitle = "";
                                if (epItem.contains("title") && !epItem["title"].is_null()) {
                                    epTitle = epItem["title"].get<std::string>();
                                }
                                
                                std::string epNum = "";
                                if (epItem.contains("episode_num") && !epItem["episode_num"].is_null()) {
                                    if (epItem["episode_num"].is_string()) epNum = epItem["episode_num"].get<std::string>();
                                    else epNum = std::to_string(epItem["episode_num"].get<int>());
                                }
                                
                                if (epTitle.empty()) {
                                    epTitle = "Episode " + epNum;
                                } else if (!epNum.empty()) {
                                    epTitle = epNum + ". " + epTitle;
                                }
                                
                                ep.title = epTitle;
                                ep.logo = seriesItem.logo;
                                ep.groupTitle = seriesItem.title + " - Season " + seasonNum;
                                
                                std::string container = "mp4";
                                if (epItem.contains("container_extension") && !epItem["container_extension"].is_null()) {
                                    container = epItem["container_extension"].get<std::string>();
                                }
                                if (container.empty()) container = "mp4";
                                
                                std::string epUrl = serverUrl;
                                if (epUrl.back() != '/') epUrl += "/";
                                epUrl += "series/" + username + "/" + password + "/" + ep.id + "." + container;
                                ep.url = epUrl;
                                
                                if (!ep.id.empty() && !ep.url.empty()) {
                                    eps.push_back(ep);
                                }
                            }
                            if (!eps.empty()) {
                                seasonNames.push_back("Season " + seasonNum);
                                episodesBySeason.push_back(std::move(eps));
                            }
                        }
                    }
                } else if (episodesJson.is_array()) {
                    std::vector<tsvitch::LiveM3u8> eps;
                    for (const auto& epItem : episodesJson) {
                        tsvitch::LiveM3u8 ep;
                        if (epItem.contains("id") && !epItem["id"].is_null()) {
                            if (epItem["id"].is_string()) ep.id = epItem["id"].get<std::string>();
                            else if (epItem["id"].is_number()) ep.id = std::to_string(epItem["id"].get<int>());
                        }
                        
                        std::string epTitle = "";
                        if (epItem.contains("title") && !epItem["title"].is_null()) {
                            epTitle = epItem["title"].get<std::string>();
                        }
                        
                        std::string epNum = "";
                        if (epItem.contains("episode_num") && !epItem["episode_num"].is_null()) {
                            if (epItem["episode_num"].is_string()) epNum = epItem["episode_num"].get<std::string>();
                            else epNum = std::to_string(epItem["episode_num"].get<int>());
                        }
                        
                        if (epTitle.empty()) {
                            epTitle = "Episode " + epNum;
                        } else if (!epNum.empty()) {
                            epTitle = epNum + ". " + epTitle;
                        }
                        
                        ep.title = epTitle;
                        ep.logo = seriesItem.logo;
                        ep.groupTitle = seriesItem.title;
                        
                        std::string container = "mp4";
                        if (epItem.contains("container_extension") && !epItem["container_extension"].is_null()) {
                            container = epItem["container_extension"].get<std::string>();
                        }
                        if (container.empty()) container = "mp4";
                        
                        std::string epUrl = serverUrl;
                        if (epUrl.back() != '/') epUrl += "/";
                        epUrl += "series/" + username + "/" + password + "/" + ep.id + "." + container;
                        ep.url = epUrl;
                        
                        if (!ep.id.empty() && !ep.url.empty()) {
                            eps.push_back(ep);
                        }
                    }
                    if (!eps.empty()) {
                        seasonNames.push_back("Episodes");
                        episodesBySeason.push_back(std::move(eps));
                    }
                }
                
                if (seasonNames.empty()) {
                    brls::sync([]() {
                        auto dialog = new brls::Dialog("No episodes found for this series");
                        dialog->addButton("OK", []() {});
                        dialog->open();
                    });
                    return;
                }
                
                brls::sync([seasonNames, episodesBySeason, onClose]() {
                    BaseDropdown::text(
                        "Select Season", seasonNames,
                        [seasonNames, episodesBySeason, onClose](int seasonIdx) {
                            const auto& eps = episodesBySeason[seasonIdx];
                            std::vector<std::string> epTitles;
                            for (const auto& ep : eps) {
                                epTitles.push_back(ep.title);
                            }
                            
                            BaseDropdown::text(
                                seasonNames[seasonIdx], epTitles,
                                [eps, onClose](int epIdx) {
                                    Intent::openLive(eps, epIdx, onClose);
                                },
                                0
                            );
                        },
                        0
                    );
                });
            } else {
                brls::sync([]() {
                    auto dialog = new brls::Dialog("No episodes info found");
                    dialog->addButton("OK", []() {});
                    dialog->open();
                });
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Exception parsing series episodes: {}", e.what());
            brls::sync([]() {
                auto dialog = new brls::Dialog("Failed to parse episodes info");
                dialog->addButton("OK", []() {});
                dialog->open();
            });
        }
    }, cpr::Url{seriesInfoUrl}, cpr::Timeout{timeoutMs});
}

<<<<<<< HEAD
void Intent::openLive(const std::vector<tsvitch::LiveM3u8>& channelList, size_t index, std::function<void()> onClose) {
    if (index < channelList.size() && pystring::startswith(channelList[index].url, "xtream-series://")) {
        showSeriesEpisodes(channelList[index], onClose);
    } else {
        auto activity = new LiveActivity(channelList, index, onClose);
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        registerFullscreen(activity);
    }
}

=======
>>>>>>> library-updates
void Intent::openSettings(std::function<void()> onClose) {
    auto activity = new SettingsActivity(onClose);
    brls::Application::pushActivity(activity);
    registerFullscreen(activity);
}

void Intent::openHint() { brls::Application::pushActivity(new HintActivity()); }

void Intent::openMain() {
    auto activity = new MainActivity();
    brls::Application::pushActivity(activity);
    registerFullscreen(activity);
}

void Intent::_registerFullscreen(brls::Activity* activity) { (void)activity; }
