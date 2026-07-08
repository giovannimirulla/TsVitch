#include "utils/xtream_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <curl/curl.h>
#include <thread>
#include <sstream>
#include <algorithm>

using namespace tsvitch;

// Callback per scrivere i dati di risposta HTTP
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
}

static std::string jsonToString(const json& obj, const std::string& key, const std::string& fallback = "") {
    if (!obj.contains(key))
        return fallback;

    const auto& v = obj.at(key);
    if (v.is_string())
        return v.get<std::string>();

    if (v.is_number_integer())
        return std::to_string(v.get<long long>());

    if (v.is_number_unsigned())
        return std::to_string(v.get<unsigned long long>());

    if (v.is_number_float()) {
        std::ostringstream oss;
        oss << v.get<double>();
        return oss.str();
    }

    if (v.is_boolean())
        return v.get<bool>() ? "true" : "false";

    return v.dump();
}

void XtreamAPI::setCredentials(const std::string& serverUrl, const std::string& username, const std::string& password) {
    this->serverUrl = serverUrl;
    this->username = username;
    this->password = password;
    
    // Rimuovi trailing slash se presente
    if (!this->serverUrl.empty() && this->serverUrl.back() == '/') {
        this->serverUrl.pop_back();
    }
    
    brls::Logger::info("XtreamAPI: Credentials set for server: {}", this->serverUrl);
}

bool XtreamAPI::isConfigured() const {
    return !serverUrl.empty() && !username.empty() && !password.empty();
}

std::string XtreamAPI::buildApiUrl(const std::string& action) const {
    if (!isConfigured()) {
        return "";
    }
    
    std::ostringstream url;
    url << serverUrl << "/player_api.php?username=" << username 
        << "&password=" << password << "&action=" << action;
    
    return url.str();
}

std::string XtreamAPI::getStreamUrl(const std::string& streamId, const std::string& extension) const {
    if (!isConfigured()) {
        return "";
    }
    
    std::ostringstream url;
    url << serverUrl << "/live/" << username << "/" << password << "/" << streamId << "." << extension;
    
    return url.str();
}

std::string XtreamAPI::getVODUrl(const std::string& streamId, const std::string& extension) const {
    if (!isConfigured()) {
        return "";
    }
    
    std::ostringstream url;
    url << serverUrl << "/movie/" << username << "/" << password << "/" << streamId << "." << extension;
    
    return url.str();
}

std::string XtreamAPI::getSeriesUrl(const std::string& seriesId, const std::string& seasonNumber, const std::string& episodeNumber, const std::string& extension) const {
    if (!isConfigured()) {
        return "";
    }
    
    std::ostringstream url;
    url << serverUrl << "/series/" << username << "/" << password << "/" << seriesId << "/" << seasonNumber << "/" << episodeNumber << "." << extension;
    
    return url.str();
}

std::string XtreamAPI::getSeriesEpisodeUrl(const std::string& episodeId, const std::string& extension) const {
    if (!isConfigured()) {
        return "";
    }

    std::ostringstream url;
    url << serverUrl << "/series/" << username << "/" << password << "/" << episodeId << "." << extension;

    return url.str();
}

void XtreamAPI::makeRequest(const std::string& url, std::function<void(const json&, bool, const std::string&)> callback) {
    // Esegui la richiesta in un thread separato per non bloccare la UI
    std::thread([url, callback]() {
        CURL* curl = curl_easy_init();
        if (!curl) {
            brls::sync([callback]() {
                callback(json{}, false, "Failed to initialize CURL");
            });
            return;
        }
        
        std::string response;
        
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // Timeout di 30 secondi
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "TsVitch/1.0");
        
        CURLcode res = curl_easy_perform(curl);
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        curl_easy_cleanup(curl);
        
        // Torna al thread principale per il callback
        brls::sync([callback, response, res, httpCode]() {
            if (res != CURLE_OK || httpCode != 200) {
                std::string error = "HTTP error: " + std::to_string(httpCode) + " - " + curl_easy_strerror(res);
                callback(json{}, false, error);
                return;
            }
            
            try {
                json jsonResponse = json::parse(response);
                callback(jsonResponse, true, "");
            } catch (const json::parse_error& e) {
                std::string error = "JSON parse error: " + std::string(e.what());
                callback(json{}, false, error);
            }
        });
    }).detach();
}

void XtreamAPI::authenticate(AuthCallback callback) {
    if (!isConfigured()) {
        callback(XtreamAuthInfo{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_account_info");
    brls::Logger::info("XtreamAPI: Authenticating with URL: {}", url);
    
    makeRequest(url, [callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(XtreamAuthInfo{}, false, error);
            return;
        }
        
        try {
            XtreamAuthInfo authInfo;
            if (response.contains("user_info")) {
                const auto& userInfo = response["user_info"];
                authInfo.status = userInfo.value("status", "");
                authInfo.exp_date = userInfo.value("exp_date", "");
                authInfo.is_trial = userInfo.value("is_trial", "");
                authInfo.active_cons = userInfo.value("active_cons", "");
                authInfo.created_at = userInfo.value("created_at", "");
                authInfo.max_connections = userInfo.value("max_connections", "");
                
                if (userInfo.contains("allowed_output_formats")) {
                    authInfo.allowed_output_formats = userInfo["allowed_output_formats"].dump();
                }
            }
            
            bool authSuccess = authInfo.status == "Active";
            authInfo.message = authSuccess ? "Authentication successful" : "Authentication failed: " + authInfo.status;
            
            callback(authInfo, authSuccess, authInfo.message);
        } catch (const std::exception& e) {
            callback(XtreamAuthInfo{}, false, "Error parsing authentication response: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getLiveTVCategories(CategoriesCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamCategory>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_live_categories");
    brls::Logger::info("XtreamAPI: Getting live TV categories from: {}", url);
    
    makeRequest(url, [callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamCategory>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamCategory> categories;
            
            if (response.is_array()) {
                for (const auto& item : response) {
                    XtreamCategory category;
                    category.category_id = jsonToString(item, "category_id");
                    category.category_name = jsonToString(item, "category_name");
                    category.parent_id = jsonToString(item, "parent_id");
                    categories.push_back(category);
                }
            }
            
            brls::Logger::info("XtreamAPI: Retrieved {} categories", categories.size());
            callback(categories, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamCategory>{}, false, "Error parsing categories: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getLiveTVChannels(const std::string& categoryId, ChannelsCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamChannel>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_live_streams");
    if (!categoryId.empty()) {
        url += "&category_id=" + categoryId;
    }
    
    brls::Logger::info("XtreamAPI: Getting live TV channels from: {}", url);
    
    makeRequest(url, [callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamChannel>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamChannel> channels;
            
            if (response.is_array()) {
                for (const auto& item : response) {
                    XtreamChannel channel;
                    channel.num = jsonToString(item, "num");
                    channel.name = jsonToString(item, "name");
                    channel.stream_type = jsonToString(item, "stream_type");
                    channel.stream_id = jsonToString(item, "stream_id");
                    channel.stream_icon = jsonToString(item, "stream_icon");
                    channel.epg_channel_id = jsonToString(item, "epg_channel_id");
                    channel.added = jsonToString(item, "added");
                    channel.category_name = jsonToString(item, "category_name");
                    channel.category_id = jsonToString(item, "category_id");
                    channel.series_no = jsonToString(item, "series_no");
                    channel.live = jsonToString(item, "live");
                    channel.container_extension = jsonToString(item, "container_extension");
                    channel.custom_sid = jsonToString(item, "custom_sid");
                    channel.tv_archive = jsonToString(item, "tv_archive");
                    channel.direct_source = jsonToString(item, "direct_source");
                    channel.tv_archive_duration = jsonToString(item, "tv_archive_duration");
                    channels.push_back(channel);
                }
            }
            
            brls::Logger::info("XtreamAPI: Retrieved {} channels", channels.size());
            callback(channels, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamChannel>{}, false, "Error parsing channels: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getAllLiveTVChannels(ChannelsCallback callback) {
    getLiveTVChannels("", callback); // Empty category ID gets all channels
}

void XtreamAPI::getVODCategories(VODCategoriesCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamVODCategory>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_vod_categories");
    brls::Logger::info("XtreamAPI: Getting VOD categories from: {}", url);
    
    makeRequest(url, [this, callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamVODCategory>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamVODCategory> categories = parseVODCategories(response);
            brls::Logger::info("XtreamAPI: Retrieved {} VOD categories", categories.size());
            callback(categories, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamVODCategory>{}, false, "Error parsing VOD categories: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getVODStreams(const std::string& categoryId, VODStreamsCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamVODStream>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_vod_streams");
    if (!categoryId.empty()) {
        url += "&category_id=" + categoryId;
    }
    
    brls::Logger::info("XtreamAPI: Getting VOD streams from: {}", url);
    
    makeRequest(url, [this, callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamVODStream>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamVODStream> streams = parseVODStreams(response);
            brls::Logger::info("XtreamAPI: Retrieved {} VOD streams", streams.size());
            callback(streams, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamVODStream>{}, false, "Error parsing VOD streams: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getSeriesCategories(SeriesCategoriesCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamSeriesCategory>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_series_categories");
    brls::Logger::info("XtreamAPI: Getting series categories from: {}", url);
    
    makeRequest(url, [this, callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamSeriesCategory>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamSeriesCategory> categories = parseSeriesCategories(response);
            brls::Logger::info("XtreamAPI: Retrieved {} series categories", categories.size());
            callback(categories, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamSeriesCategory>{}, false, "Error parsing series categories: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getSeriesByCategory(const std::string& categoryId, SeriesListCallback callback) {
    if (!isConfigured()) {
        callback(std::vector<XtreamSeries>{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_series");
    if (!categoryId.empty()) {
        url += "&category_id=" + categoryId;
    }
    
    brls::Logger::info("XtreamAPI: Getting series by category from: {}", url);
    
    makeRequest(url, [this, callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(std::vector<XtreamSeries>{}, false, error);
            return;
        }
        
        try {
            std::vector<XtreamSeries> series = parseSeries(response);
            brls::Logger::info("XtreamAPI: Retrieved {} series", series.size());
            callback(series, true, "");
        } catch (const std::exception& e) {
            callback(std::vector<XtreamSeries>{}, false, "Error parsing series: " + std::string(e.what()));
        }
    });
}

void XtreamAPI::getSeriesInfo(const std::string& seriesId, SeriesInfoCallback callback) {
    if (!isConfigured()) {
        callback(XtreamSeriesInfo{}, false, "Xtream credentials not configured");
        return;
    }
    
    std::string url = buildApiUrl("get_series_info");
    url += "&series_id=" + seriesId;
    
    brls::Logger::info("XtreamAPI: Getting series info from: {}", url);
    
    makeRequest(url, [this, callback](const json& response, bool success, const std::string& error) {
        if (!success) {
            callback(XtreamSeriesInfo{}, false, error);
            return;
        }
        
        try {
            XtreamSeriesInfo info = parseSeriesInfo(response);
            brls::Logger::info("XtreamAPI: Retrieved series info");
            callback(info, true, "");
        } catch (const std::exception& e) {
            callback(XtreamSeriesInfo{}, false, "Error parsing series info: " + std::string(e.what()));
        }
    });
}

std::vector<XtreamVODCategory> XtreamAPI::parseVODCategories(const json& data) {
    std::vector<XtreamVODCategory> categories;
    
    if (data.is_array()) {
        for (const auto& item : data) {
            XtreamVODCategory category;
            category.category_id = jsonToString(item, "category_id");
            category.category_name = jsonToString(item, "category_name");
            category.parent_id = jsonToString(item, "parent_id");
            category.category_image = jsonToString(item, "category_image");
            categories.push_back(category);
        }
    }
    
    return categories;
}

std::vector<XtreamVODStream> XtreamAPI::parseVODStreams(const json& data) {
    std::vector<XtreamVODStream> streams;
    
    if (data.is_array()) {
        for (const auto& item : data) {
            XtreamVODStream stream;
            stream.num = item.value("num", 0);
            stream.stream_id = jsonToString(item, "stream_id");
            stream.name = jsonToString(item, "name");
            stream.stream_icon = jsonToString(item, "stream_icon");
            stream.rating = jsonToString(item, "rating");
            stream.container_extension = jsonToString(item, "container_extension");
            stream.custom_sid = jsonToString(item, "custom_sid");
            streams.push_back(stream);
        }
    }
    
    return streams;
}

std::vector<XtreamSeriesCategory> XtreamAPI::parseSeriesCategories(const json& data) {
    std::vector<XtreamSeriesCategory> categories;
    
    if (data.is_array()) {
        for (const auto& item : data) {
            XtreamSeriesCategory category;
            category.category_id = jsonToString(item, "category_id");
            category.category_name = jsonToString(item, "category_name");
            category.parent_id = jsonToString(item, "parent_id");
            categories.push_back(category);
        }
    }
    
    return categories;
}

std::vector<XtreamSeries> XtreamAPI::parseSeries(const json& data) {
    std::vector<XtreamSeries> seriesList;
    
    if (data.is_array()) {
        for (const auto& item : data) {
            XtreamSeries series;
            series.num = item.value("num", 0);
            series.series_id = jsonToString(item, "series_id");
            series.name = jsonToString(item, "name");
            series.plot = jsonToString(item, "plot");
            series.cover = jsonToString(item, "cover");
            series.cast = jsonToString(item, "cast");
            series.director = jsonToString(item, "director");
            series.genre = jsonToString(item, "genre");
            series.rating = jsonToString(item, "rating");
            series.rating_5based = jsonToString(item, "rating_5based");
            series.releaseDate = jsonToString(item, "releaseDate");
            series.episode_run_time = jsonToString(item, "episode_run_time");
            series.category_id = jsonToString(item, "category_id");
            seriesList.push_back(series);
        }
    }
    
    return seriesList;
}

XtreamSeriesInfo XtreamAPI::parseSeriesInfo(const json& data) {
    XtreamSeriesInfo info{};

    // get_series_info returns an object with fields "info", "episodes", "seasons"
    const json* infoJson = nullptr;
    if (data.contains("info")) infoJson = &data.at("info");
    if (data.is_array() && !data.empty() && data[0].is_object() && data[0].contains("info")) infoJson = &data[0].at("info");

    if (infoJson) {
        info.num = infoJson->value("num", 0);
        info.series_id = jsonToString(*infoJson, "series_id");
        info.name = jsonToString(*infoJson, "name");
        info.plot = jsonToString(*infoJson, "plot");
        info.cover = jsonToString(*infoJson, "cover");
        info.cast = jsonToString(*infoJson, "cast");
        info.director = jsonToString(*infoJson, "director");
        info.genre = jsonToString(*infoJson, "genre");
        info.rating = jsonToString(*infoJson, "rating");
        info.rating_5based = jsonToString(*infoJson, "rating_5based");
        info.releaseDate = jsonToString(*infoJson, "releaseDate");
        info.episode_run_time = jsonToString(*infoJson, "episode_run_time");
        info.category_id = jsonToString(*infoJson, "category_id");

        if (infoJson->contains("backdrop_path")) {
            const auto& backdrop = (*infoJson)["backdrop_path"];
            if (backdrop.is_array()) {
                for (const auto& path : backdrop) {
                    info.backdrop_path.push_back(path.get<std::string>());
                }
            } else if (backdrop.is_string()) {
                info.backdrop_path.push_back(backdrop.get<std::string>());
            }
        }
    }

    if (data.contains("seasons")) {
        info.seasons = parseSeasons(data.at("seasons"));
    }

    if (data.contains("episodes")) {
        auto episodesSeasons = parseEpisodes(data.at("episodes"));
        for (auto& epsSeason : episodesSeasons) {
            auto it = std::find_if(info.seasons.begin(), info.seasons.end(), [&](const XtreamSeason& s) {
                return s.season_number == epsSeason.season_number;
            });

            if (it != info.seasons.end()) {
                it->episodes = std::move(epsSeason.episodes);
            } else {
                info.seasons.push_back(std::move(epsSeason));
            }
        }
    }

    return info;
}

std::vector<XtreamSeason> XtreamAPI::parseSeasons(const json& data) {
    std::vector<XtreamSeason> seasons;
    if (data.is_array()) {
        for (const auto& item : data) {
            XtreamSeason s;
            s.season_number = jsonToString(item, "season_number", jsonToString(item, "name"));
            seasons.push_back(s);
        }
    }
    return seasons;
}

std::vector<XtreamSeason> XtreamAPI::parseEpisodes(const json& data) {
    std::vector<XtreamSeason> seasons;
    // data is an object with season keys mapping to array of episodes
    if (data.is_object()) {
        for (auto it = data.begin(); it != data.end(); ++it) {
            XtreamSeason season;
            season.season_number = it.key();
            const auto& epsArr = it.value();
            if (!epsArr.is_array()) continue;
            for (const auto& ep : epsArr) {
                XtreamEpisode e;
                e.season = season.season_number;
                e.id = jsonToString(ep, "id", jsonToString(ep, "episode_id"));
                e.title = jsonToString(ep, "title");
                e.episode_num = jsonToString(ep, "episode_num");
                e.container_extension = jsonToString(ep, "container_extension", "mkv");
                e.custom_sid = jsonToString(ep, "custom_sid");
                e.direct_source = jsonToString(ep, "direct_source");
                if (ep.contains("info") && ep["info"].is_object()) {
                    e.duration = jsonToString(ep["info"], "duration");
                }
                season.episodes.push_back(e);
            }
            seasons.push_back(std::move(season));
        }
    }
    return seasons;
}
