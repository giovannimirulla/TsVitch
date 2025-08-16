#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>
#include <vector>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>

#include "tsvitch.h"
#include "tsvitch/util/http.hpp"
#include "borealis/core/thread.hpp"

#include "tsvitch/result/home_live_result.h"
#include "utils/config_helper.hpp"

namespace tsvitch {

/// Pulisce il testo rimuovendo caratteri che potrebbero causare problemi di rendering
// Helper function to safely get string value from JSON, handling null values
static std::string safeGetString(const nlohmann::json& json, const std::string& key, const std::string& defaultValue = "") {
    if (!json.contains(key) || json[key].is_null()) {
        return defaultValue;
    }
    if (json[key].is_string()) {
        return json[key].get<std::string>();
    }
    return defaultValue;
}

// Helper function to sanitize text for safe font rendering
static std::string sanitizeText(const std::string& text) {
    if (text.empty()) return text;
    
    std::string cleaned = text;
    
    // Rimuovi caratteri di controllo ASCII (0-31, eccetto tab, newline, carriage return)
    cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), [](unsigned char c) {
        return (c < 32 && c != 9 && c != 10 && c != 13) || c == 127;
    }), cleaned.end());
    
    // Sostituisci caratteri problematici con versioni sicure
    std::replace(cleaned.begin(), cleaned.end(), '\n', ' ');
    std::replace(cleaned.begin(), cleaned.end(), '\r', ' ');
    std::replace(cleaned.begin(), cleaned.end(), '\t', ' ');
    
    // Rimuovi spazi multipli
    std::regex multiple_spaces("\\s+");
    cleaned = std::regex_replace(cleaned, multiple_spaces, " ");
    
    // Trim
    cleaned.erase(0, cleaned.find_first_not_of(" \t\n\r\f\v"));
    cleaned.erase(cleaned.find_last_not_of(" \t\n\r\f\v") + 1);
    
    return cleaned;
}

/// Découpe une chaîne `a,b,c` -> {"a","b","c"}
static std::vector<std::string> split_csv(const std::string& csv)
{
    std::vector<std::string> out;
    std::stringstream        ss(csv);
    std::string              item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

nlohmann::json parse_m3u8_to_json(const std::string& m3u8_content)
{
    std::istringstream stream(m3u8_content);
    std::string        line;
    nlohmann::json     json_result = nlohmann::json::array();
    nlohmann::json     current_entry;

    while (std::getline(stream, line)) {
        if (line.rfind("#EXTINF:", 0) == 0) {
            if (!current_entry.empty()) {
                json_result.push_back(current_entry);
                current_entry.clear();
            }

            std::string extinf = line.substr(8);
            size_t      id_pos = extinf.find("tvg-id=\"");
            size_t      chno_pos = extinf.find("tvg-chno=\"");
            size_t      logo_pos = extinf.find("tvg-logo=\"");
            size_t      group_title_pos = extinf.find("group-title=\"");
            size_t      comma_pos = extinf.find_last_of(',');

            if (id_pos != std::string::npos) {
                size_t end_pos     = extinf.find('"', id_pos + 8);
                std::string id_val = extinf.substr(id_pos + 8, end_pos - (id_pos + 8));
                current_entry["id"]   = id_val;
                current_entry["tags"] = split_csv(id_val);     // <- NOUVEAU
            }
            if (chno_pos != std::string::npos) {
                size_t end_pos        = extinf.find('"', chno_pos + 10);
                current_entry["chno"] = extinf.substr(chno_pos + 10, end_pos - (chno_pos + 10));
            }
            if (logo_pos != std::string::npos) {
                size_t end_pos        = extinf.find('"', logo_pos + 10);
                current_entry["logo"] = extinf.substr(logo_pos + 10, end_pos - (logo_pos + 10));
            }
            if (group_title_pos != std::string::npos) {
                size_t end_pos              = extinf.find('"', group_title_pos + 13);
                std::string groupTitle = extinf.substr(group_title_pos + 13, end_pos - (group_title_pos + 13));
                current_entry["groupTitle"] = sanitizeText(groupTitle);
            }
            if (comma_pos != std::string::npos) {
                std::string title = extinf.substr(comma_pos + 1);
                current_entry["title"] = sanitizeText(title);
            }
        } else if (!line.empty() && line.rfind('#', 0) != 0) {
            current_entry["url"] = line;
        }
    }

    if (!current_entry.empty() && current_entry.contains("id"))
        json_result.push_back(current_entry);
    

#ifdef DEBUG
    std::cout << "json_result:\n" << json_result.dump(2) << std::endl;
#endif
    return json_result;
}

void TsVitchClient::get_file_m3u8(const std::function<void(LiveM3u8ListResult)>& callback,
                                  const ErrorCallback&                           error)
{
    auto m3u8Url = ProgramConfig::instance().getM3U8Url();
    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    HTTP::__cpr_get(
        m3u8Url,
        {},
        timeoutMs, // timeout configurabile per file M3U8 grandi
        [callback, error](const cpr::Response& r) {
            try {
                nlohmann::json json_result = parse_m3u8_to_json(r.text);

                LiveM3u8ListResult result;
                for (const auto& item : json_result) {
                    LiveM3u8 live;
                    live.id         = item.value("id", "");
                    live.chno       = item.value("chno", "");
                    live.logo       = item.value("logo", "");
                    {
                        std::string groupTitle = item.value("groupTitle", "");
                        std::replace(groupTitle.begin(), groupTitle.end(), ';', ' ');
                        live.groupTitle = sanitizeText(groupTitle);
                    }
                    live.title      = sanitizeText(item.value("title", ""));
                    live.url        = item.value("url", "");
                    result.push_back(live);
                }
                CALLBACK(result);
            } catch (const std::exception&) {
                ERROR_MSG("cannot get file m3u8", -1);
                //return error
                error("Failed to parse m3u8 content", -1);
            }
        },
        error);
}

void TsVitchClient::get_xtream_channels(const std::function<void(LiveM3u8ListResult)>& callback,
                                       const ErrorCallback& error) {
    get_xtream_channels_with_retry(callback, error, 3); // Max 3 retry attempts
}

void TsVitchClient::get_xtream_channels_with_retry(const std::function<void(LiveM3u8ListResult)>& callback,
                                                  const ErrorCallback& error, int maxRetries) {
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username = ProgramConfig::instance().getXtreamUsername();
    auto password = ProgramConfig::instance().getXtreamPassword();
    
    if (serverUrl.empty() || username.empty() || password.empty()) {
        if (error) {
            error("Xtream Codes credentials not configured properly", -1);
        }
        return;
    }
    
    // Construct Xtream API URL for getting all live streams
    std::string xtreamUrl = serverUrl;
    if (xtreamUrl.back() != '/') {
        xtreamUrl += "/";
    }
    xtreamUrl += "player_api.php?username=" + username + "&password=" + password + "&action=get_live_streams";
    
    brls::Logger::debug("Fetching Xtream channels from: {} (retries left: {})", xtreamUrl, maxRetries);
    
    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    
    // Use cpr::Get directly to bypass the strict status code check in __cpr_get
    cpr::GetCallback(
        [callback, error, maxRetries, xtreamUrl, timeoutMs, serverUrl, username, password](const cpr::Response& r) {
            try {
                brls::Logger::debug("Xtream response status: {}, body length: {}", r.status_code, r.text.length());
                
                // Handle network errors
                if (r.error) {
                    brls::Logger::error("Xtream network error: {}", r.error.message);
                    if (maxRetries > 0) {
                        brls::Logger::info("Retrying Xtream request due to network error (retries left: {})", maxRetries - 1);
                        brls::Threading::async([callback, error, maxRetries]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000)); // Wait 1 second
                            TsVitchClient::get_xtream_channels_with_retry(callback, error, maxRetries - 1);
                        });
                        return;
                    }
                    if (error) {
                        error("Network error: " + r.error.message, -1);
                    }
                    return;
                }
                
                // Handle HTTP errors with retry for 503
                if (r.status_code == 503 && maxRetries > 0) {
                    brls::Logger::warning("Xtream server returned 503 Service Unavailable, retrying in 2 seconds (retries left: {})", maxRetries - 1);
                    brls::Threading::async([callback, error, maxRetries]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(2000)); // Wait 2 seconds for 503
                        TsVitchClient::get_xtream_channels_with_retry(callback, error, maxRetries - 1);
                    });
                    return;
                }
                
                if (r.status_code != 200) {
                    brls::Logger::error("Xtream API error: HTTP {}, body: {}", r.status_code, r.text.substr(0, 500));
                    if (error) {
                        error("HTTP error " + std::to_string(r.status_code) + ": " + r.text.substr(0, 200), r.status_code);
                    }
                    return;
                }
                
                if (r.text.empty()) {
                    brls::Logger::error("Xtream API returned empty response");
                    if (error) {
                        error("Empty response from Xtream server", -1);
                    }
                    return;
                }
                
                nlohmann::json json_result;
                try {
                    json_result = nlohmann::json::parse(r.text);
                } catch (const nlohmann::json::parse_error& e) {
                    brls::Logger::error("Failed to parse Xtream JSON: {}", e.what());
                    if (error) {
                        error("Invalid JSON response from Xtream server", -1);
                    }
                    return;
                }
                
                if (!json_result.is_array()) {
                    brls::Logger::error("Xtream response is not an array, type: {}", json_result.type_name());
                    if (error) {
                        error("Invalid response format from Xtream server", -1);
                    }
                    return;
                }
                
                LiveM3u8ListResult result;
                for (size_t i = 0; i < json_result.size(); i++) {
                    const auto& item = json_result[i];
                    if (!item.is_object()) {
                        brls::Logger::debug("Skipping non-object item at index {} in Xtream response", i);
                        continue;
                    }
                    
                    try {
                        LiveM3u8 live;
                        
                        // Log first few items for debugging structure
                        if (i < 3) {
                            brls::Logger::debug("Xtream item {}: {}", i, item.dump());
                        }
                    
                    // Map Xtream fields to our structure with safe access
                    // Handle both string and number types for IDs, and null values
                    if (item.contains("stream_id") && !item["stream_id"].is_null()) {
                        if (item["stream_id"].is_string()) {
                            live.id = item["stream_id"].get<std::string>();
                        } else if (item["stream_id"].is_number()) {
                            live.id = std::to_string(item["stream_id"].get<int>());
                        }
                    }
                    
                    // Handle both string and number types for channel numbers, and null values
                    if (item.contains("num") && !item["num"].is_null()) {
                        if (item["num"].is_string()) {
                            live.chno = item["num"].get<std::string>();
                        } else if (item["num"].is_number()) {
                            live.chno = std::to_string(item["num"].get<int>());
                        }
                    }
                    
                    // Handle string fields with null-safe access
                    live.title = sanitizeText(safeGetString(item, "name"));
                    live.logo = safeGetString(item, "stream_icon");
                    
                    std::string categoryName = safeGetString(item, "category_name");
                    brls::Logger::info("Xtream channel '{}' has category_name: '{}'", live.title, categoryName);
                    live.groupTitle = sanitizeText(categoryName);
                    
                    // If category_name is empty, set a default group
                    if (live.groupTitle.empty()) {
                        live.groupTitle = "Live TV";
                        brls::Logger::info("Set default group 'Live TV' for channel '{}'", live.title);
                    }
                    
                    // Construct the stream URL for Xtream
                    if (!live.id.empty()) {
                        std::string streamUrl = serverUrl;
                        if (streamUrl.back() != '/') {
                            streamUrl += "/";
                        }
                        streamUrl += "live/" + username + "/" + password + "/" + live.id + ".ts";
                        live.url = streamUrl;
                    }
                    
                    // Only add channels that have required fields
                    if (!live.id.empty() && !live.title.empty() && !live.url.empty()) {
                        result.push_back(live);
                    }
                    } catch (const std::exception& e) {
                        brls::Logger::error("Exception processing Xtream item at index {}: {}", i, e.what());
                        brls::Logger::error("Problematic item: {}", item.dump());
                        // Continue with next item instead of failing completely
                        continue;
                    }
                }
                
                brls::Logger::debug("Successfully parsed {} Xtream channels", result.size());
                if (callback) {
                    callback(result);
                }
            } catch (const std::exception& e) {
                brls::Logger::error("Exception in Xtream response handler: {}", e.what());
                if (error) {
                    error("Failed to parse Xtream channels response: " + std::string(e.what()), -1);
                }
            }
        },
        cpr::Url{xtreamUrl},
        cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
        cpr::Timeout{timeoutMs},
        HTTP::HEADERS,
        HTTP::COOKIES,
        HTTP::PROXIES,
        HTTP::VERIFY);
}

void TsVitchClient::get_live_channels(const std::function<void(LiveM3u8ListResult)>& callback,
                                     const ErrorCallback& error) {
    // Check IPTV mode and call appropriate function
    int iptvMode = ProgramConfig::instance().getIntOption(SettingItem::IPTV_MODE);
    
    if (iptvMode == 0) {
        // M3U8 Mode
        brls::Logger::debug("Using M3U8 mode for live channels");
        get_file_m3u8(callback, error);
    } else if (iptvMode == 1) {
        // Xtream Codes Mode
        brls::Logger::debug("Using Xtream mode for live channels");
        get_xtream_channels(callback, error);
    } else {
        // Unknown mode
        brls::Logger::error("Unknown IPTV mode: {}", iptvMode);
        if (error) {
            error("Unknown IPTV mode configured", -1);
        }
    }
}

} // namespace tsvitch
