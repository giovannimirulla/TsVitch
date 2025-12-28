#pragma once

#include <string>
#include <vector>
#include <functional>
#include <nlohmann/json.hpp>
#include <borealis/core/singleton.hpp>

using json = nlohmann::json;

namespace tsvitch {

struct XtreamChannel {
    std::string num;
    std::string name;
    std::string stream_type;
    std::string stream_id;
    std::string stream_icon;
    std::string epg_channel_id;
    std::string added;
    std::string category_name;
    std::string category_id;
    std::string series_no;
    std::string live;
    std::string container_extension;
    std::string custom_sid;
    std::string tv_archive;
    std::string direct_source;
    std::string tv_archive_duration;
};

struct XtreamCategory {
    std::string category_id;
    std::string category_name;
    std::string parent_id;
};

struct XtreamAuthInfo {
    std::string status;
    std::string message;
    std::string exp_date;
    std::string is_trial;
    std::string active_cons;
    std::string created_at;
    std::string max_connections;
    std::string allowed_output_formats;
};

class XtreamAPI : public brls::Singleton<XtreamAPI> {
public:
    using ChannelsCallback = std::function<void(const std::vector<XtreamChannel>&, bool success, const std::string& error)>;
    using CategoriesCallback = std::function<void(const std::vector<XtreamCategory>&, bool success, const std::string& error)>;
    using AuthCallback = std::function<void(const XtreamAuthInfo&, bool success, const std::string& error)>;

    XtreamAPI() = default;
    ~XtreamAPI() = default;

    // Configura le credenziali Xtream
    void setCredentials(const std::string& serverUrl, const std::string& username, const std::string& password);
    
    // Verifica l'autenticazione
    void authenticate(AuthCallback callback);
    
    // Ottieni le categorie live TV
    void getLiveTVCategories(CategoriesCallback callback);
    
    // Ottieni i canali live TV per categoria
    void getLiveTVChannels(const std::string& categoryId, ChannelsCallback callback);
    
    // Ottieni tutti i canali live TV
    void getAllLiveTVChannels(ChannelsCallback callback);
    
    // Genera l'URL dello stream per un canale
    std::string getStreamUrl(const std::string& streamId, const std::string& extension = "ts") const;
    
    // Verifica se le credenziali sono configurate
    bool isConfigured() const;

private:
    std::string serverUrl;
    std::string username;
    std::string password;
    
    // Costruisce l'URL base per le API
    std::string buildApiUrl(const std::string& action) const;
    
    // Esegue una richiesta HTTP asincrona
    void makeRequest(const std::string& url, std::function<void(const json&, bool, const std::string&)> callback);
    
    // Parsing helper functions
    std::vector<XtreamChannel> parseChannels(const json& data);
    std::vector<XtreamCategory> parseCategories(const json& data);
    XtreamAuthInfo parseAuthInfo(const json& data);
};

} // namespace tsvitch
