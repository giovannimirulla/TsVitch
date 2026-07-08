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

struct XtreamVODCategory {
    std::string category_id;
    std::string category_name;
    std::string parent_id;
    std::string category_image;
};

struct XtreamVODStream {
    int num;
    std::string stream_id;
    std::string name;
    std::string stream_icon;
    std::string rating;
    std::string container_extension;
    std::string custom_sid;
};

struct XtreamSeriesCategory {
    std::string category_id;
    std::string category_name;
    std::string parent_id;
};

struct XtreamEpisode {
    std::string season;
    std::string id;
    std::string title;
    std::string episode_num;
    std::string container_extension;
    std::string custom_sid;
    std::string direct_source;
    std::string duration;
};

struct XtreamSeason {
    std::string season_number;
    std::vector<XtreamEpisode> episodes;
};

struct XtreamSeriesInfo {
    int num;
    std::string series_id;
    std::string name;
    std::string plot;
    std::string cover;
    std::string cast;
    std::string director;
    std::string genre;
    std::string rating;
    std::string rating_5based;
    std::string releaseDate;
    std::string episode_run_time;
    std::string category_id;
    std::vector<std::string> backdrop_path;
    std::vector<XtreamSeason> seasons;
};

struct XtreamSeries {
    int num;
    std::string series_id;
    std::string name;
    std::string plot;
    std::string cover;
    std::string cast;
    std::string director;
    std::string genre;
    std::string rating;
    std::string rating_5based;
    std::string releaseDate;
    std::string episode_run_time;
    std::string category_id;
};

class XtreamAPI : public brls::Singleton<XtreamAPI> {
public:
    using ChannelsCallback = std::function<void(const std::vector<XtreamChannel>&, bool success, const std::string& error)>;
    using CategoriesCallback = std::function<void(const std::vector<XtreamCategory>&, bool success, const std::string& error)>;
    using AuthCallback = std::function<void(const XtreamAuthInfo&, bool success, const std::string& error)>;
    using VODCategoriesCallback = std::function<void(const std::vector<XtreamVODCategory>&, bool success, const std::string& error)>;
    using VODStreamsCallback = std::function<void(const std::vector<XtreamVODStream>&, bool success, const std::string& error)>;
    using SeriesCategoriesCallback = std::function<void(const std::vector<XtreamSeriesCategory>&, bool success, const std::string& error)>;
    using SeriesListCallback = std::function<void(const std::vector<XtreamSeries>&, bool success, const std::string& error)>;
    using SeriesInfoCallback = std::function<void(const XtreamSeriesInfo&, bool success, const std::string& error)>;

    XtreamAPI() = default;
    ~XtreamAPI() = default;

    // Configura le credenziali Xtream
    void setCredentials(const std::string& serverUrl, const std::string& username, const std::string& password);
    
    // Verifica l'autenticazione
    void authenticate(AuthCallback callback);
    
    // LIVE TV
    void getLiveTVCategories(CategoriesCallback callback);
    void getLiveTVChannels(const std::string& categoryId, ChannelsCallback callback);
    void getAllLiveTVChannels(ChannelsCallback callback);
    
    // VOD
    void getVODCategories(VODCategoriesCallback callback);
    void getVODStreams(const std::string& categoryId, VODStreamsCallback callback);
    
    // SERIES
    void getSeriesCategories(SeriesCategoriesCallback callback);
    void getSeriesByCategory(const std::string& categoryId, SeriesListCallback callback);
    void getSeriesInfo(const std::string& seriesId, SeriesInfoCallback callback);
    
    // Genera l'URL dello stream per un canale/VOD
    std::string getStreamUrl(const std::string& streamId, const std::string& extension = "ts") const;
    std::string getVODUrl(const std::string& streamId, const std::string& extension = "mkv") const;
    std::string getSeriesUrl(const std::string& seriesId, const std::string& seasonNumber, const std::string& episodeNumber, const std::string& extension = "mkv") const;
    std::string getSeriesEpisodeUrl(const std::string& episodeId, const std::string& extension = "mkv") const;
    
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
    std::vector<XtreamVODCategory> parseVODCategories(const json& data);
    std::vector<XtreamVODStream> parseVODStreams(const json& data);
    std::vector<XtreamSeriesCategory> parseSeriesCategories(const json& data);
    std::vector<XtreamSeries> parseSeries(const json& data);
    XtreamSeriesInfo parseSeriesInfo(const json& data);
    std::vector<XtreamSeason> parseSeasons(const json& data);
    std::vector<XtreamSeason> parseEpisodes(const json& data);
    XtreamAuthInfo parseAuthInfo(const json& data);
};

} // namespace tsvitch
