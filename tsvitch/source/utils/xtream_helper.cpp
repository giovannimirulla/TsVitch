#include "utils/xtream_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <curl/curl.h>
#include <thread>
#include <sstream>

using namespace tsvitch;

// Callback per scrivere i dati di risposta HTTP
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp) {
    userp->append((char*)contents, size * nmemb);
    return size * nmemb;
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
                    category.category_id = item.value("category_id", "");
                    category.category_name = item.value("category_name", "");
                    category.parent_id = item.value("parent_id", "");
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
                    channel.num = item.value("num", "");
                    channel.name = item.value("name", "");
                    channel.stream_type = item.value("stream_type", "");
                    channel.stream_id = item.value("stream_id", "");
                    channel.stream_icon = item.value("stream_icon", "");
                    channel.epg_channel_id = item.value("epg_channel_id", "");
                    channel.added = item.value("added", "");
                    channel.category_name = item.value("category_name", "");
                    channel.category_id = item.value("category_id", "");
                    channel.series_no = item.value("series_no", "");
                    channel.live = item.value("live", "");
                    channel.container_extension = item.value("container_extension", "");
                    channel.custom_sid = item.value("custom_sid", "");
                    channel.tv_archive = item.value("tv_archive", "");
                    channel.direct_source = item.value("direct_source", "");
                    channel.tv_archive_duration = item.value("tv_archive_duration", "");
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
