#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>
#include <vector>
#include <algorithm>
#include <regex>
#include <thread>
#include <chrono>
#include <unordered_map>
#include <memory>

#include "tsvitch.h"
#include "tsvitch/util/http.hpp"
#include "borealis/core/thread.hpp"
#include "borealis/core/application.hpp"

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

// Extracts a field that may be a string or a number (e.g. category_id) as a string
static std::string safeGetIdString(const nlohmann::json& json, const std::string& key) {
    if (!json.contains(key) || json[key].is_null()) {
        return "";
    }
    if (json[key].is_string()) {
        return json[key].get<std::string>();
    }
    if (json[key].is_number_integer()) {
        return std::to_string(json[key].get<long long>());
    }
    if (json[key].is_number()) {
        return std::to_string(json[key].get<int>());
    }
    return "";
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
    if (m3u8_content.empty()) {
        return nlohmann::json::array();
    }

    std::istringstream stream(m3u8_content);
    std::string        line;
    nlohmann::json     json_result = nlohmann::json::array();
    nlohmann::json     current_entry;
    
    // Pre-allocazione ottimizzata per migliorare prestazioni
    size_t estimated_channels = std::count(m3u8_content.begin(), m3u8_content.end(), '\n') / 3;
    json_result.get_ref<nlohmann::json::array_t&>().reserve(estimated_channels + 100);

    // Buffer riutilizzabile per evitare allocazioni
    std::string extinf_buffer;
    extinf_buffer.reserve(512);

    while (std::getline(stream, line)) {
        // Trim veloce di \r\n alla fine
        while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        
        // Controllo ultra-rapido per saltare linee vuote o commenti non EXTINF
        if (line.empty() || (line[0] == '#' && (line.size() < 8 || line.compare(0, 8, "#EXTINF:") != 0))) {
            continue;
        }
        
        if (line.compare(0, 8, "#EXTINF:") == 0) {
            if (!current_entry.empty()) {
                json_result.push_back(std::move(current_entry));
                current_entry.clear();
            }

            extinf_buffer = line.substr(8);
            
            // Parsing ultra-ottimizzato con una sola passata della stringa
            const char* data = extinf_buffer.c_str();
            const size_t len = extinf_buffer.length();
            
            // Cerca i pattern più comuni per primi (ottimizzazione basata su frequenza)
            size_t comma_pos = std::string::npos;
            size_t tvg_id_start = 0, tvg_id_len = 0;
            size_t tvg_chno_start = 0, tvg_chno_len = 0;
            size_t tvg_logo_start = 0, tvg_logo_len = 0;
            size_t group_title_start = 0, group_title_len = 0;
            
            for (size_t i = 0; i < len; ++i) {
                // Cerca comma (titolo) - più frequente
                if (data[i] == ',' && comma_pos == std::string::npos) {
                    comma_pos = i;
                }
                // Cerca tvg-id
                else if (data[i] == 't' && i + 7 < len && memcmp(data + i, "tvg-id=\"", 8) == 0) {
                    i += 8;
                    tvg_id_start = i;
                    while (i < len && data[i] != '"') ++i;
                    tvg_id_len = i - tvg_id_start;
                }
                // Cerca group-title
                else if (data[i] == 'g' && i + 12 < len && memcmp(data + i, "group-title=\"", 13) == 0) {
                    i += 13;
                    group_title_start = i;
                    while (i < len && data[i] != '"') ++i;
                    group_title_len = i - group_title_start;
                }
                // Cerca tvg-chno
                else if (data[i] == 't' && i + 9 < len && memcmp(data + i, "tvg-chno=\"", 10) == 0) {
                    i += 10;
                    tvg_chno_start = i;
                    while (i < len && data[i] != '"') ++i;
                    tvg_chno_len = i - tvg_chno_start;
                }
                // Cerca tvg-logo
                else if (data[i] == 't' && i + 9 < len && memcmp(data + i, "tvg-logo=\"", 10) == 0) {
                    i += 10;
                    tvg_logo_start = i;
                    while (i < len && data[i] != '"') ++i;
                    tvg_logo_len = i - tvg_logo_start;
                }
            }

            // Assegna i valori trovati (solo se non vuoti)
            if (tvg_id_len > 0) {
                current_entry["id"] = std::string(data + tvg_id_start, tvg_id_len);
            }
            if (tvg_chno_len > 0) {
                current_entry["chno"] = std::string(data + tvg_chno_start, tvg_chno_len);
            }
            if (tvg_logo_len > 0) {
                current_entry["logo"] = std::string(data + tvg_logo_start, tvg_logo_len);
            }
            if (group_title_len > 0) {
                current_entry["groupTitle"] = sanitizeText(std::string(data + group_title_start, group_title_len));
            }
            if (comma_pos != std::string::npos && comma_pos + 1 < len) {
                current_entry["title"] = sanitizeText(extinf_buffer.substr(comma_pos + 1));
            }
        } else if (!line.empty() && line[0] != '#') {
            current_entry["url"] = std::move(line);
        }
    }

    // Aggiungi l'ultimo entry se presente e valido
    if (!current_entry.empty() && current_entry.contains("id")) {
        json_result.push_back(std::move(current_entry));
    }
    

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
    
    // Timeout più intelligente basato sulla dimensione prevista
    if (timeoutMs < 30000) timeoutMs = 30000; // Minimum 30 secondi per file M3U8 grandi
    
    brls::Logger::info("Fetching M3U8 playlist from: {} (timeout: {}ms)", m3u8Url, timeoutMs);
    
    HTTP::__cpr_get(
        m3u8Url,
        {},
        timeoutMs,
        [callback, error](const cpr::Response& r) {
            // Log dimensione risposta per debug prestazioni
            brls::Logger::info("M3U8 download completed - Size: {} bytes, Status: {}", r.text.size(), r.status_code);
            
#ifdef __SWITCH__
            // Per file molto grandi, usa sempre parsing asincrono anche su Switch
            bool useAsyncParsing = r.text.size() > 1024 * 1024; // 1MB threshold
            
            // Switch: usa parsing sincrono per file piccoli, asincrono per file grandi
            if (useAsyncParsing) {
                brls::Logger::info("Large M3U8 file detected ({}MB), using async parsing on Switch", r.text.size() / (1024*1024));
                
                // Create a cancellation token 
                auto cancellationToken = std::make_shared<std::atomic<bool>>(false);
                
                // Subscribe to exit event to cancel parsing
                auto exitSubscription = brls::Application::getExitEvent()->subscribe([cancellationToken]() {
                    brls::Logger::info("M3U8 parsing: Exit event received, setting cancellation flag");
                    cancellationToken->store(true);
                });
                
                brls::Threading::async([callback, error, responseText = std::move(r.text), cancellationToken, exitSubscription]() {
                    try {
                        // Check if parsing should be canceled
                        if (cancellationToken->load()) {
                            brls::Logger::info("M3U8 async parsing canceled before start - application is exiting");
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                            return;
                        }
                        
                        auto start_time = std::chrono::high_resolution_clock::now();
                        nlohmann::json json_result = parse_m3u8_to_json(responseText);
                        
                        // Check again if parsing should be canceled
                        if (cancellationToken->load()) {
                            brls::Logger::info("M3U8 async parsing canceled during execution - application is exiting");
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                            return;
                        }
                        
                        auto end_time = std::chrono::high_resolution_clock::now();
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                        brls::Logger::info("Switch async M3U8 parsing completed in {}ms, found {} channels", duration.count(), json_result.size());

                        LiveM3u8ListResult result;
                        result.reserve(json_result.size());
                        
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
                            result.push_back(std::move(live));
                        }
                        
                        // Final check before calling callback
                        if (cancellationToken->load()) {
                            brls::Logger::info("M3U8 async parsing canceled before callback - application is exiting");
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                            return;
                        }
                        
                        brls::sync([callback, result = std::move(result), cancellationToken, exitSubscription]() {
                            // Check once more in sync context
                            if (cancellationToken->load()) {
                                brls::Logger::info("M3U8 sync callback canceled - application is exiting");
                                brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                                return;
                            }
                            CALLBACK(result);
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                        });
                    } catch (const std::exception& e) {
                        brls::Logger::error("Switch async M3U8 parsing error: {}", e.what());
                        
                        // Don't call error callback if app is exiting
                        if (cancellationToken->load()) {
                            brls::Logger::info("M3U8 async error callback canceled - application is exiting");
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                            return;
                        }
                        
                        brls::sync([error, cancellationToken, exitSubscription]() {
                            if (cancellationToken->load()) {
                                brls::Logger::info("M3U8 sync error callback canceled - application is exiting");
                                brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                                return;
                            }
                            ERROR_MSG("cannot get file m3u8", -1);
                            error("Failed to parse m3u8 content", -1);
                            brls::Application::getExitEvent()->unsubscribe(exitSubscription);
                        });
                    }
                });
            } else {
                // File piccoli: parsing sincrono ottimizzato
                try {
                    auto start_time = std::chrono::high_resolution_clock::now();
                    nlohmann::json json_result = parse_m3u8_to_json(r.text);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    brls::Logger::info("Switch sync M3U8 parsing completed in {}ms, found {} channels", duration.count(), json_result.size());

                    LiveM3u8ListResult result;
                    result.reserve(json_result.size());
                    
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
                        result.push_back(std::move(live));
                    }
                    
                    CALLBACK(result);
                } catch (const std::exception& e) {
                    brls::Logger::error("Switch sync M3U8 parsing error: {}", e.what());
                    ERROR_MSG("cannot get file m3u8", -1);
                    error("Failed to parse m3u8 content", -1);
                }
            }
#else
            // Altre piattaforme: sempre parsing asincrono per migliori prestazioni
            brls::Threading::async([callback, error, responseText = std::move(r.text)]() {
                try {
                    auto start_time = std::chrono::high_resolution_clock::now();
                    nlohmann::json json_result = parse_m3u8_to_json(responseText);
                    auto end_time = std::chrono::high_resolution_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
                    brls::Logger::info("M3U8 parsing completed in {}ms, found {} channels", duration.count(), json_result.size());

                    LiveM3u8ListResult result;
                    result.reserve(json_result.size());
                    
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
                        result.push_back(std::move(live));
                    }
                    
                    brls::sync([callback, result = std::move(result)]() {
                        CALLBACK(result);
                    });
                } catch (const std::exception& e) {
                    brls::Logger::error("M3U8 parsing error: {}", e.what());
                    brls::sync([error]() {
                        ERROR_MSG("cannot get file m3u8", -1);
                        error("Failed to parse m3u8 content", -1);
                    });
                }
            });
#endif
        },
        error);
}

void TsVitchClient::get_xtream_channels(const std::function<void(LiveM3u8ListResult)>& callback,
                                       const ErrorCallback& error) {
    get_xtream_channels_with_retry(callback, error, 3); // Max 3 retry attempts
}

// Shared category_id -> category_name map of the Xtream categories
using XtreamCategoryMap = std::shared_ptr<std::unordered_map<std::string, std::string>>;

/**
 * Describes which Xtream content to fetch. Live TV and Movies (VOD) share the exact
 * same fetch/parse pipeline and only differ in the API actions, the stream URL path
 * segment and the file extension, so a single implementation is parameterized by this.
 */
struct XtreamContentKind {
    std::string categoriesAction;    // get_live_categories / get_vod_categories / get_series_categories
    std::string streamsAction;       // get_live_streams / get_vod_streams / get_series
    std::string urlSegment;          // "live" / "movie" (não usado para séries)
    bool useContainerExtension;      // false -> ".ts"; true -> item.container_extension (fallback mp4)
    std::string fallbackGroupTitle;  // group name when the category cannot be resolved
    std::string label;               // used only in logs
    bool isSeriesList;               // true -> lista de séries (id=series_id, sem url de player)
};

// Esquema de URL usado como sentinela para itens de série na lista (o clique abre os episódios)
static const std::string XTREAM_SERIES_SCHEME = "xtream-series://";

static const XtreamContentKind XTREAM_LIVE{
    "get_live_categories", "get_live_streams", "live", false, "Live TV", "live", false};
static const XtreamContentKind XTREAM_MOVIES{
    "get_vod_categories", "get_vod_streams", "movie", true, "Movies", "vod", false};
static const XtreamContentKind XTREAM_SERIES{
    "get_series_categories", "get_series", "series", true, "Series", "series", true};

/**
 * Fetches the Xtream categories for the given content kind and builds a
 * category_id -> category_name map. On error it returns an empty map: the stream
 * fetch still proceeds using a fallback grouping.
 */
static void fetchXtreamCategories(const std::string& serverUrl, const std::string& username,
                                  const std::string& password, long timeoutMs,
                                  const XtreamContentKind& kind,
                                  const std::function<void(XtreamCategoryMap)>& done) {
    std::string url = serverUrl;
    if (url.back() != '/') url += "/";
    url += "player_api.php?username=" + username + "&password=" + password + "&action=" + kind.categoriesAction;

    brls::Logger::debug("Fetching Xtream {} categories from: {}", kind.label, url);

    cpr::GetCallback(
        [done, label = kind.label](const cpr::Response& r) {
            auto categories = std::make_shared<std::unordered_map<std::string, std::string>>();
            try {
                if (!r.error && r.status_code == 200 && !r.text.empty()) {
                    auto json_result = nlohmann::json::parse(r.text, nullptr, false);
                    if (json_result.is_array()) {
                        for (const auto& item : json_result) {
                            if (!item.is_object()) continue;
                            std::string id = safeGetIdString(item, "category_id");
                            std::string name = sanitizeText(safeGetString(item, "category_name"));
                            if (!id.empty() && !name.empty()) {
                                (*categories)[id] = name;
                            }
                        }
                    }
                } else {
                    brls::Logger::warning("Xtream {} categories fetch failed (status={}), continuing without category names",
                                          label, r.status_code);
                }
            } catch (const std::exception& e) {
                brls::Logger::warning("Xtream categories parse error: {}, continuing without category names", e.what());
            }
            brls::Logger::info("Xtream: loaded {} {} categories", categories->size(), label);
            brls::sync([done, categories]() { done(categories); });
        },
        cpr::Url{url},
        cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
        cpr::Timeout{timeoutMs},
        HTTP::HEADERS,
        HTTP::COOKIES,
        HTTP::PROXIES,
        HTTP::VERIFY);
}

// Forward declaration of the stream fetch with an already-resolved category map
static void xtreamFetchStreamsWithRetry(const std::function<void(tsvitch::LiveM3u8ListResult)>& callback,
                                        const ErrorCallback& error, int maxRetries,
                                        XtreamCategoryMap categories, XtreamContentKind kind);

/**
 * Fetches Xtream content (Live TV or Movies) with automatic retry logic.
 * Timeout: 45+ seconds (Xtream servers typically slower than M3U8 sources)
 * Retries: 3 attempts with exponential backoff (1s network, 3s server errors)
 * Error Handling: Network errors retry, server errors 502/503 retry, 4xx no retry
 *
 * Before the streams it fetches the categories in order to resolve
 * category_id -> category_name (get_*_streams only exposes category_id),
 * enabling per-category grouping in the UI.
 */
static void xtreamFetchContentWithRetry(const std::function<void(tsvitch::LiveM3u8ListResult)>& callback,
                                        const ErrorCallback& error, int maxRetries,
                                        const XtreamContentKind& kind) {
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username = ProgramConfig::instance().getXtreamUsername();
    auto password = ProgramConfig::instance().getXtreamPassword();

    if (serverUrl.empty() || username.empty() || password.empty()) {
        if (error) {
            error("Xtream Codes credentials not configured properly", -1);
        }
        return;
    }

    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    if (timeoutMs < 45000) timeoutMs = 45000;

    // Categories first, then streams (fetched once, reused across all retries)
    fetchXtreamCategories(serverUrl, username, password, timeoutMs, kind,
                          [callback, error, maxRetries, kind](XtreamCategoryMap categories) {
        xtreamFetchStreamsWithRetry(callback, error, maxRetries, categories, kind);
    });
}

void TsVitchClient::get_xtream_channels_with_retry(const std::function<void(LiveM3u8ListResult)>& callback,
                                                  const ErrorCallback& error, int maxRetries) {
    xtreamFetchContentWithRetry(callback, error, maxRetries, XTREAM_LIVE);
}

void TsVitchClient::get_xtream_vod(const std::function<void(LiveM3u8ListResult)>& callback,
                                   const ErrorCallback& error) {
    xtreamFetchContentWithRetry(callback, error, 3, XTREAM_MOVIES);
}

void TsVitchClient::get_xtream_series(const std::function<void(LiveM3u8ListResult)>& callback,
                                      const ErrorCallback& error) {
    xtreamFetchContentWithRetry(callback, error, 3, XTREAM_SERIES);
}

/**
 * Busca os episódios de uma série (action=get_series_info). A resposta é um objeto
 * { seasons, info, episodes: { "<season>": [ {id, title, container_extension, ...} ] } }.
 * Converte em LiveM3u8ListResult onde cada episódio é reproduzível
 * (url = series/<user>/<pass>/<id>.<ext>) e agrupado por temporada.
 */
void TsVitchClient::get_xtream_series_info(const std::string& seriesId,
                                           const std::function<void(LiveM3u8ListResult)>& callback,
                                           const ErrorCallback& error) {
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username  = ProgramConfig::instance().getXtreamUsername();
    auto password  = ProgramConfig::instance().getXtreamPassword();

    if (serverUrl.empty() || username.empty() || password.empty() || seriesId.empty()) {
        if (error) error("Xtream Codes credentials not configured properly", -1);
        return;
    }

    std::string url = serverUrl;
    if (url.back() != '/') url += "/";
    url += "player_api.php?username=" + username + "&password=" + password +
           "&action=get_series_info&series_id=" + seriesId;

    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    if (timeoutMs < 45000) timeoutMs = 45000;

    brls::Logger::debug("Fetching Xtream series info: {}", url);

    cpr::GetCallback(
        [callback, error, serverUrl, username, password](const cpr::Response& r) {
            if (r.error || r.status_code != 200 || r.text.empty()) {
                brls::Logger::error("Xtream series_info error: status={}", r.status_code);
                if (error) error("Failed to fetch series episodes", r.status_code);
                return;
            }
            brls::Threading::async([callback, error, responseText = std::move(r.text), serverUrl, username, password]() {
                try {
                    auto d = nlohmann::json::parse(responseText, nullptr, false);
                    if (d.is_discarded() || !d.is_object() || !d.contains("episodes") || !d["episodes"].is_object()) {
                        brls::sync([error]() { if (error) error("Invalid series episodes response", -1); });
                        return;
                    }

                    // Nomes das temporadas (season_number -> name), se disponíveis
                    std::unordered_map<std::string, std::string> seasonNames;
                    if (d.contains("seasons") && d["seasons"].is_array()) {
                        for (const auto& s : d["seasons"]) {
                            if (!s.is_object()) continue;
                            std::string num  = safeGetIdString(s, "season_number");
                            std::string name = sanitizeText(safeGetString(s, "name"));
                            if (!num.empty() && !name.empty()) seasonNames[num] = name;
                        }
                    }

                    std::string base = serverUrl;
                    if (base.back() != '/') base += "/";

                    LiveM3u8ListResult episodes;
                    const auto& eps = d["episodes"];
                    for (auto it = eps.begin(); it != eps.end(); ++it) {
                        const std::string& seasonKey = it.key();
                        if (!it.value().is_array()) continue;
                        std::string seasonLabel = seasonNames.count(seasonKey) ? seasonNames[seasonKey]
                                                                              : ("Temporada " + seasonKey);
                        for (const auto& ep : it.value()) {
                            if (!ep.is_object()) continue;
                            std::string id  = safeGetIdString(ep, "id");
                            if (id.empty()) continue;
                            std::string ext = safeGetString(ep, "container_extension");
                            if (ext.empty()) ext = "mp4";

                            LiveM3u8 e;
                            e.id         = id;
                            e.title      = sanitizeText(safeGetString(ep, "title"));
                            if (e.title.empty()) e.title = seasonLabel + " - " + safeGetIdString(ep, "episode_num");
                            e.groupTitle = sanitizeText(seasonLabel);
                            e.url        = base + "series/" + username + "/" + password + "/" + id + "." + ext;
                            episodes.push_back(std::move(e));
                        }
                    }

                    brls::Logger::info("Xtream: parsed {} episodes", episodes.size());
                    brls::sync([callback, episodes = std::move(episodes)]() {
                        if (callback) callback(episodes);
                    });
                } catch (const std::exception& e) {
                    brls::sync([error, e]() { if (error) error(std::string("Series parse error: ") + e.what(), -1); });
                }
            });
        },
        cpr::Url{url},
        cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
        cpr::Timeout{timeoutMs},
        HTTP::HEADERS,
        HTTP::COOKIES,
        HTTP::PROXIES,
        HTTP::VERIFY);
}

/**
 * Actual stream fetch. Receives the already-resolved category map and content kind;
 * retries call this function again without re-downloading the categories.
 */
static void xtreamFetchStreamsWithRetry(const std::function<void(tsvitch::LiveM3u8ListResult)>& callback,
                                        const ErrorCallback& error, int maxRetries,
                                        XtreamCategoryMap categories, XtreamContentKind kind) {
    auto serverUrl = ProgramConfig::instance().getXtreamServerUrl();
    auto username = ProgramConfig::instance().getXtreamUsername();
    auto password = ProgramConfig::instance().getXtreamPassword();

    // Construct Xtream API URL for getting all streams of this content kind
    std::string xtreamUrl = serverUrl;
    if (xtreamUrl.back() != '/') {
        xtreamUrl += "/";
    }
    xtreamUrl += "player_api.php?username=" + username + "&password=" + password + "&action=" + kind.streamsAction;

    brls::Logger::debug("Fetching Xtream {} from: {} (retries left: {})", kind.label, xtreamUrl, maxRetries);

    auto timeoutMs = ProgramConfig::instance().getIntOption(SettingItem::M3U8_TIMEOUT);
    // Use longer timeout for Xtream API (typically slower than M3U8 sources)
    if (timeoutMs < 45000) timeoutMs = 45000; // Minimum 45 seconds for Xtream

    // Use cpr::GetCallback per migliori prestazioni asincrono
    cpr::GetCallback(
        [callback, error, maxRetries, xtreamUrl, timeoutMs, serverUrl, username, password, categories, kind](const cpr::Response& r) {
            try {
                brls::Logger::info("Xtream response: status={}, size={}KB", r.status_code, r.text.length()/1024);
                
                // Handle network errors
                if (r.error) {
                    brls::Logger::error("Xtream network error: {}", r.error.message);
                    if (maxRetries > 0) {
                        brls::Logger::info("Retrying Xtream request due to network error (retries left: {})", maxRetries - 1);
                        brls::Threading::async([callback, error, maxRetries, categories, kind]() {
                            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                            xtreamFetchStreamsWithRetry(callback, error, maxRetries - 1, categories, kind);
                        });
                        return;
                    }
                    if (error) {
                        error("Network error: " + r.error.message, -1);
                    }
                    return;
                }
                
                // Handle HTTP errors with retry for 503 and 502
                if ((r.status_code == 503 || r.status_code == 502) && maxRetries > 0) {
                    brls::Logger::warning("Xtream server returned {} - server temporarily unavailable, retrying in 3 seconds (retries left: {})", r.status_code, maxRetries - 1);
                    brls::Threading::async([callback, error, maxRetries, categories, kind]() {
                        std::this_thread::sleep_for(std::chrono::milliseconds(3000)); // Wait 3 seconds for server errors
                        xtreamFetchStreamsWithRetry(callback, error, maxRetries - 1, categories, kind);
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
                
                // Sposta il parsing JSON in un thread asincrono per non bloccare la UI
                brls::Threading::async([callback, error, responseText = std::move(r.text), serverUrl, username, password, categories, kind]() {
                    try {
                        auto parse_start = std::chrono::high_resolution_clock::now();
                        
                        nlohmann::json json_result;
                        try {
                            json_result = nlohmann::json::parse(responseText);
                        } catch (const nlohmann::json::parse_error& e) {
                            brls::Logger::error("Failed to parse Xtream JSON: {}", e.what());
                            brls::sync([error]() {
                                if (error) error("Invalid JSON response from Xtream server", -1);
                            });
                            return;
                        }
                        
                        if (!json_result.is_array()) {
                            brls::Logger::error("Xtream response is not an array, type: {}", json_result.type_name());
                            brls::sync([error]() {
                                if (error) error("Invalid response format from Xtream server", -1);
                            });
                            return;
                        }
                        
                        LiveM3u8ListResult result;
                        result.reserve(json_result.size()); // Pre-allocazione per prestazioni
                        
                        size_t processed = 0, skipped = 0;
                        for (size_t i = 0; i < json_result.size(); i++) {
                            const auto& item = json_result[i];
                            if (!item.is_object()) {
                                skipped++;
                                continue;
                            }
                            
                            try {
                                LiveM3u8 live;

                                // Séries usam series_id + cover; Live/Filmes usam stream_id + stream_icon
                                if (kind.isSeriesList) {
                                    live.id   = safeGetIdString(item, "series_id");
                                    live.logo = safeGetString(item, "cover");
                                } else {
                                    live.id = safeGetIdString(item, "stream_id");
                                    live.logo = safeGetString(item, "stream_icon");
                                }

                                if (item.contains("num") && !item["num"].is_null()) {
                                    if (item["num"].is_string()) {
                                        live.chno = item["num"].get<std::string>();
                                    } else if (item["num"].is_number()) {
                                        live.chno = std::to_string(item["num"].get<int>());
                                    }
                                }

                                live.title = sanitizeText(safeGetString(item, "name"));
                                
                                // get_*_streams only exposes category_id: resolve the name from the
                                // category map; fall back to category_name (if present) and then the kind default.
                                std::string categoryName = safeGetString(item, "category_name");
                                if (categoryName.empty() && categories) {
                                    std::string categoryId = safeGetIdString(item, "category_id");
                                    if (!categoryId.empty()) {
                                        auto it = categories->find(categoryId);
                                        if (it != categories->end()) {
                                            categoryName = it->second;
                                        }
                                    }
                                }
                                live.groupTitle = sanitizeText(categoryName.empty() ? kind.fallbackGroupTitle : categoryName);

                                // URL do item.
                                // Séries: sentinela xtream-series://<id> (o clique abre os episódios).
                                // Live -> live/.../id.ts ; Filmes (VOD) -> movie/.../id.<container_extension>
                                if (!live.id.empty()) {
                                    if (kind.isSeriesList) {
                                        live.url = XTREAM_SERIES_SCHEME + live.id;
                                    } else {
                                        std::string streamUrl = serverUrl;
                                        if (streamUrl.back() != '/') {
                                            streamUrl += "/";
                                        }
                                        std::string ext;
                                        if (kind.useContainerExtension) {
                                            ext = safeGetString(item, "container_extension");
                                            if (ext.empty()) ext = "mp4";
                                        } else {
                                            ext = "ts";
                                        }
                                        streamUrl += kind.urlSegment + "/" + username + "/" + password + "/" + live.id + "." + ext;
                                        live.url = streamUrl;
                                    }
                                }
                                
                                // Only add channels that have required fields
                                if (!live.id.empty() && !live.title.empty() && !live.url.empty()) {
                                    result.push_back(std::move(live));
                                    processed++;
                                }
                                
                            } catch (const std::exception& e) {
                                brls::Logger::error("Exception processing Xtream item at index {}: {}", i, e.what());
                                skipped++;
                                continue;
                            }
                        }
                        
                        auto parse_end = std::chrono::high_resolution_clock::now();
                        auto parse_duration = std::chrono::duration_cast<std::chrono::milliseconds>(parse_end - parse_start);
                        brls::Logger::info("Xtream parsing completed in {}ms - processed: {}, skipped: {}, total: {}", 
                                         parse_duration.count(), processed, skipped, json_result.size());
                        
                        brls::sync([callback, result = std::move(result)]() {
                            if (callback) {
                                callback(result);
                            }
                        });
                        
                    } catch (const std::exception& e) {
                        brls::Logger::error("Exception in Xtream async processing: {}", e.what());
                        brls::sync([error, e]() {
                            if (error) {
                                error("Failed to parse Xtream channels response: " + std::string(e.what()), -1);
                            }
                        });
                    }
                });
            } catch (const std::exception& e) {
                brls::Logger::error("Exception in Xtream response handler: {}", e.what());
                if (error) {
                    error("Failed to process Xtream response: " + std::string(e.what()), -1);
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
        // Xtream Codes Mode: choose between Live TV and Movies (VOD)
        int contentType = ProgramConfig::instance().getXtreamContentType();
        if (contentType == 1) {
            brls::Logger::debug("Using Xtream mode for movies (VOD)");
            get_xtream_vod(callback, error);
        } else if (contentType == 2) {
            brls::Logger::debug("Using Xtream mode for series");
            get_xtream_series(callback, error);
        } else {
            brls::Logger::debug("Using Xtream mode for live channels");
            get_xtream_channels(callback, error);
        }
    } else {
        // Unknown mode
        brls::Logger::error("Unknown IPTV mode: {}", iptvMode);
        if (error) {
            error("Unknown IPTV mode configured", -1);
        }
    }
}

} // namespace tsvitch
