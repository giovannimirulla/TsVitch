#pragma once

#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <unordered_set>
#include <nlohmann/json.hpp>
#include "analytics.h"
#include "borealis/core/singleton.hpp"
#include "borealis/core/logger.hpp"

#ifdef PS4
const std::string primaryDNSStr   = "223.5.5.5";
const std::string secondaryDNSStr = "1.1.1.1";
#endif

typedef std::map<std::string, std::string> Cookie;
constexpr uint32_t MINIMUM_WINDOW_WIDTH  = 480;
constexpr uint32_t MINIMUM_WINDOW_HEIGHT = 270;

enum class SettingItem {
    HIDE_BOTTOM_BAR,
    HIDE_FPS,
    FULLSCREEN,
    MINIMUM_WINDOW_WIDTH,
    MINIMUM_WINDOW_HEIGHT,
    ON_TOP_WINDOW_WIDTH,
    ON_TOP_WINDOW_HEIGHT,
    ON_TOP_MODE,
    APP_THEME,
    APP_LANG,
    APP_RESOURCES,
    APP_UI_SCALE,
    APP_SWAP_ABXY,
    SCROLL_SPEED,
    HISTORY_REPORT,
    PLAYER_AUTO_PLAY,
    PLAYER_STRATEGY,
    PLAYER_BOTTOM_BAR,
    PLAYER_HIGHLIGHT_BAR,
    PLAYER_SKIP_OPENING_CREDITS,
    PLAYER_LOW_QUALITY,
    PLAYER_INMEMORY_CACHE,
    PLAYER_HWDEC,
    PLAYER_HWDEC_CUSTOM,
    PLAYER_EXIT_FULLSCREEN_ON_END,
    PLAYER_DEFAULT_SPEED,
    PLAYER_VOLUME,
    PLAYER_ASPECT,
    PLAYER_BRIGHTNESS,
    PLAYER_CONTRAST,
    PLAYER_SATURATION,
    PLAYER_HUE,
    PLAYER_GAMMA,
    PLAYER_OSD_TV_MODE,
    VIDEO_QUALITY,
    TEXTURE_CACHE_NUM,
    OPENCC_ON,
    CUSTOM_UPDATE_API,
    IMAGE_REQUEST_THREADS,
    
    GAMEPAD_VIBRATION,
    KEYMAP,
    HOME_WINDOW_STATE,
    SEARCH_TV_MODE,
    LIMITED_FPS,
    DEACTIVATED_TIME,
    DEACTIVATED_FPS,
    DLNA_IP,
    DLNA_PORT,
    DLNA_NAME,

    M3U8_URL_ITEM,
    PROXY_URL_ITEM,
    M3U8_TIMEOUT,

    TLS_VERIFY,
    UP_FILTER,

    // IPTV Mode Selection
    IPTV_MODE,  // 0 = M3U8, 1 = Xtream

    // Xtream Codes IPTV Settings
    XTREAM_SERVER_URL,
    XTREAM_USERNAME,
    XTREAM_PASSWORD,
    XTREAM_ENABLED,

    GROUP_SELECTED_INDEX,
};

class APPVersion : public brls::Singleton<APPVersion> {
    inline static std::string RELEASE_API = "https://api.github.com/repos/giovannimirulla/TsVitch/releases/latest";

public:
    int major, minor, revision;
    std::string git_commit, git_tag;

    APPVersion();

    std::string getVersionStr();

    std::string getPlatform();

    static std::string getPackageName();

    bool needUpdate(std::string latestVersion);

    void checkUpdate(int delay = 2000, bool showUpToDateDialog = false);
};

class CustomTheme {
public:
    std::string id;
    std::string name;
    std::string desc;
    std::string version;
    std::string author;
    std::string path;
};
inline void from_json(const nlohmann::json& nlohmann_json_j, CustomTheme& nlohmann_json_t) {
    if (nlohmann_json_j.contains("name") && nlohmann_json_j.at("name").is_string())
        nlohmann_json_j.at("name").get_to(nlohmann_json_t.name);
    if (nlohmann_json_j.contains("desc") && nlohmann_json_j.at("desc").is_string())
        nlohmann_json_j.at("desc").get_to(nlohmann_json_t.desc);
    if (nlohmann_json_j.contains("version") && nlohmann_json_j.at("version").is_string())
        nlohmann_json_j.at("version").get_to(nlohmann_json_t.version);
    if (nlohmann_json_j.contains("author") && nlohmann_json_j.at("author").is_string())
        nlohmann_json_j.at("author").get_to(nlohmann_json_t.author);
}


typedef struct ProgramOption {
    std::string key;

    std::vector<std::string> optionList;

    std::vector<int> rawOptionList;

    size_t defaultOption;
} ProgramOption;

class ProgramConfig : public brls::Singleton<ProgramConfig> {
public:
    ProgramConfig();
    ~ProgramConfig();
    ProgramConfig(const ProgramConfig& config);
    void setProgramConfig(const ProgramConfig& conf);

    std::string getClientID();

    std::string getDeviceID();

    void setDeviceID(const std::string& deviceId);

    void loadHomeWindowState();
    void saveHomeWindowState();

    template <typename T>
    T getSettingItem(SettingItem item, T defaultValue) {
        auto& key = SETTING_MAP[item].key;
        if (!setting.contains(key)) return defaultValue;
        try {
            return this->setting.at(key).get<T>();
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", key, e.what());
            return defaultValue;
        }
    }

    template <typename T>
    void setSettingItem(SettingItem item, T data, bool save = true) {
        setting[SETTING_MAP[item].key] = data;
        if (save) this->save();
    }

    ProgramOption getOptionData(SettingItem item);

    /**
     * 获取 int 类型选项的当前设定值的索引
     */
    size_t getIntOptionIndex(SettingItem item);

    /**
     * 获取 int 类型选项的当前设定值
     */
    int getIntOption(SettingItem item);

    bool getBoolOption(SettingItem item);

    int getStringOptionIndex(SettingItem item);

    void load();

    void save();

    void init();

    std::string getConfigDir();

    std::string getHomePath();

    void exit(char* argv[]);

    void loadCustomThemes();

    std::vector<CustomTheme> getCustomThemes();

    void toggleFullscreen();

    void checkOnTop();

    std::string getM3U8Url();

    void setM3U8Url(const std::string& url);

    // Xtream Codes IPTV methods
    std::string getXtreamServerUrl();
    void setXtreamServerUrl(const std::string& url);
    std::string getXtreamUsername();
    void setXtreamUsername(const std::string& username);
    std::string getXtreamPassword();
    void setXtreamPassword(const std::string& password);
    bool getXtreamEnabled();
    void setXtreamEnabled(bool enabled);

    std::string getProxyUrl();

    void setProxyUrl(const std::string& url);

    std::vector<CustomTheme> customThemes;
    nlohmann::json setting;
    std::string client;
    std::string device;
    std::string m3u8Url;
    std::string proxyUrl;
    brls::Event<>::Subscription exitEventSubscription;
    bool hasExitSubscription = false;
    static std::unordered_map<SettingItem, ProgramOption> SETTING_MAP;
};

inline void to_json(nlohmann::json& nlohmann_json_j, const ProgramConfig& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, setting, client, device));
}

inline void from_json(const nlohmann::json& nlohmann_json_j, ProgramConfig& nlohmann_json_t) {
    if (nlohmann_json_j.contains("setting")) nlohmann_json_j.at("setting").get_to(nlohmann_json_t.setting);
    if (nlohmann_json_j.contains("client") && nlohmann_json_j.at("client").is_string())
        nlohmann_json_j.at("client").get_to(nlohmann_json_t.client);
    if (nlohmann_json_j.contains("device") && nlohmann_json_j.at("device").is_string())
        nlohmann_json_j.at("device").get_to(nlohmann_json_t.device);
}

class Register {
public:
    static void initCustomView();
    static void initCustomTheme();
    static void initCustomStyle();
};
