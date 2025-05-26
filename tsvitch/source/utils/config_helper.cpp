

#ifdef IOS
#include <CoreFoundation/CoreFoundation.h>
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include <unistd.h>
#include <borealis/platforms/desktop/desktop_platform.hpp>
#if defined(_WIN32)
#include <shlobj.h>
#endif
#endif

#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/core/touch/pan_gesture.hpp>
#include <borealis/views/edit_text_dialog.hpp>
#include <cpr/filesystem.h>

#include "tsvitch.h"
#include "utils/number_helper.hpp"
#include "utils/thread_helper.hpp"
#include "utils/image_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/crash_helper.hpp"
#include "utils/vibration_helper.hpp"
#include "activity/live_player_activity.hpp"
#include "view/video_view.hpp"
#include "view/mpv_core.hpp"

#include "config/m3u8_config.h"

#ifdef PS4
#include <orbis/SystemService.h>
#include <orbis/Sysmodule.h>
#include <arpa/inet.h>

extern "C" {
extern int ps4_mpv_use_precompiled_shaders;
extern int ps4_mpv_dump_shaders;
extern in_addr_t primary_dns;
extern in_addr_t secondary_dns;
}
#endif

#ifdef _WIN32
#include <winsock2.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 256
#endif

using namespace brls::literals;

std::unordered_map<SettingItem, ProgramOption> ProgramConfig::SETTING_MAP = {

    {SettingItem::CUSTOM_UPDATE_API, {"custom_update_api", {}, {}, 0}},
    {SettingItem::APP_LANG,
     {"app_lang",
      {
#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
          brls::LOCALE_AUTO,
#endif
          brls::LOCALE_EN_US, brls::LOCALE_JA, brls::LOCALE_RYU, brls::LOCALE_ZH_HANT, brls::LOCALE_ZH_HANS,
          brls::LOCALE_Ko, brls::LOCALE_IT, brls::LOCALE_PT_BR},
      {},
#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
      0}},
#else
      4}},
#endif
    {SettingItem::APP_THEME, {"app_theme", {"auto", "light", "dark"}, {}, 0}},
    {SettingItem::APP_RESOURCES, {"app_resources", {}, {}, 0}},
    {SettingItem::APP_UI_SCALE,
     {"app_ui_scale",
      {"544p", "720p", "900p", "1080p"},
      {},
#ifdef __PSV__
      0}},
#else
      1}},
#endif
    {SettingItem::KEYMAP, {"keymap", {"xbox", "ps", "keyboard"}, {}, 0}},
    {SettingItem::HOME_WINDOW_STATE, {"home_window_state", {}, {}, 0}},
    {SettingItem::DLNA_IP, {"dlna_ip", {}, {}, 0}},
    {SettingItem::DLNA_NAME, {"dlna_name", {}, {}, 0}},
    {SettingItem::PLAYER_ASPECT, {"player_aspect", {"-1", "-2", "-3", "4:3", "16:9"}, {}, 0}},

    {SettingItem::APP_SWAP_ABXY, {"app_swap_abxy", {}, {}, 0}},
    {SettingItem::GAMEPAD_VIBRATION, {"gamepad_vibration", {}, {}, 1}},
#if defined(IOS) || defined(__PSV__)
    {SettingItem::HIDE_BOTTOM_BAR, {"hide_bottom_bar", {}, {}, 1}},
#else
    {SettingItem::HIDE_BOTTOM_BAR, {"hide_bottom_bar", {}, {}, 0}},
#endif
    {SettingItem::HIDE_FPS, {"hide_fps", {}, {}, 1}},
#if defined(__APPLE__) || !defined(NDEBUG)

    {SettingItem::FULLSCREEN, {"fullscreen", {}, {}, 0}},
#else

    {SettingItem::FULLSCREEN, {"fullscreen", {}, {}, 1}},
#endif
    {SettingItem::HISTORY_REPORT, {"history_report", {}, {}, 1}},
    {SettingItem::PLAYER_AUTO_PLAY, {"player_auto_play", {}, {}, 1}},
    {SettingItem::PLAYER_BOTTOM_BAR, {"player_bottom_bar", {}, {}, 1}},
    {SettingItem::PLAYER_HIGHLIGHT_BAR, {"player_highlight_bar", {}, {}, 0}},
    {SettingItem::PLAYER_SKIP_OPENING_CREDITS, {"player_skip_opening_credits", {}, {}, 1}},
    {SettingItem::PLAYER_LOW_QUALITY, {"player_low_quality", {}, {}, 1}},
#if defined(IOS) || defined(__PSV__) || defined(__SWITCH__)
    {SettingItem::PLAYER_HWDEC, {"player_hwdec", {}, {}, 1}},
#else
    {SettingItem::PLAYER_HWDEC, {"player_hwdec", {}, {}, 0}},
#endif
    {SettingItem::PLAYER_HWDEC_CUSTOM, {"player_hwdec_custom", {}, {}, 0}},
    {SettingItem::PLAYER_EXIT_FULLSCREEN_ON_END, {"player_exit_fullscreen_on_end", {}, {}, 1}},
    {SettingItem::PLAYER_OSD_TV_MODE, {"player_osd_tv_mode", {}, {}, 0}},
    {SettingItem::OPENCC_ON, {"opencc", {}, {}, 1}},

    {SettingItem::SEARCH_TV_MODE, {"search_tv_mode", {}, {}, 1}},
    {SettingItem::TLS_VERIFY,
     {"tls_verify",
      {},
      {},
#if defined(__PSV__) || defined(__SWITCH__) || defined(PS4)
      0}},
#else
      1}},
#endif

#if defined(__PSV__)
    {SettingItem::PLAYER_INMEMORY_CACHE, {"player_inmemory_cache", {"0MB", "1MB", "5MB", "10MB"}, {0, 1, 5, 10}, 0}},
#elif defined(__SWITCH__)
    {SettingItem::PLAYER_INMEMORY_CACHE,
     {"player_inmemory_cache", {"0MB", "10MB", "20MB", "50MB", "100MB"}, {0, 10, 20, 50, 100}, 0}},
#else
    {SettingItem::PLAYER_INMEMORY_CACHE,
     {"player_inmemory_cache", {"0MB", "10MB", "20MB", "50MB", "100MB"}, {0, 10, 20, 50, 100}, 1}},
#endif
    {
        SettingItem::PLAYER_DEFAULT_SPEED,
        {"player_default_speed",
         {"4.0x", "3.0x", "2.0x", "1.75x", "1.5x", "1.25x", "1.0x", "0.75x", "0.5x", "0.25x"},
         {400, 300, 200, 175, 150, 125, 100, 75, 50, 25},
         2},
    },
    {SettingItem::PLAYER_VOLUME, {"player_volume", {}, {}, 0}},
    {SettingItem::TEXTURE_CACHE_NUM, {"texture_cache_num", {}, {}, 0}},
    {SettingItem::VIDEO_QUALITY, {"video_quality", {}, {}, 116}},
    {SettingItem::IMAGE_REQUEST_THREADS,
     {"image_request_threads",
#if defined(__SWITCH__) || defined(__PSV__)
      {"1", "2", "3", "4"},
      {1, 2, 3, 4},
      1}},
#else
      {"1", "2", "3", "4", "8", "12", "16"},
      {1, 2, 3, 4, 8, 12, 16},
      3}},
#endif

    {SettingItem::LIMITED_FPS, {"limited_fps", {"0", "30", "60", "90", "120"}, {0, 30, 60, 90, 120}, 0}},
    {SettingItem::DEACTIVATED_TIME, {"deactivated_time", {}, {}, 0}},
    {SettingItem::DEACTIVATED_FPS, {"deactivated_fps", {}, {}, 0}},
    {SettingItem::DLNA_PORT, {"dlna_port", {}, {}, 0}},
    {SettingItem::PLAYER_STRATEGY, {"player_strategy", {"rcmd", "next", "loop", "single"}, {0, 1, 2, 3}, 0}},
    {SettingItem::PLAYER_BRIGHTNESS, {"player_brightness", {}, {}, 0}},
    {SettingItem::PLAYER_CONTRAST, {"player_contrast", {}, {}, 0}},
    {SettingItem::PLAYER_SATURATION, {"player_saturation", {}, {}, 0}},
    {SettingItem::PLAYER_HUE, {"player_hue", {}, {}, 0}},
    {SettingItem::PLAYER_GAMMA, {"player_gamma", {}, {}, 0}},
    {SettingItem::MINIMUM_WINDOW_WIDTH, {"minimum_window_width", {"480"}, {480}, 0}},
    {SettingItem::MINIMUM_WINDOW_HEIGHT, {"minimum_window_height", {"270"}, {270}, 0}},
    {SettingItem::ON_TOP_WINDOW_WIDTH, {"on_top_window_width", {"480"}, {480}, 0}},
    {SettingItem::ON_TOP_WINDOW_HEIGHT, {"on_top_window_height", {"270"}, {270}, 0}},
    {SettingItem::ON_TOP_MODE, {"on_top_mode", {"off", "always", "auto"}, {0, 1, 2}, 0}},
    {SettingItem::SCROLL_SPEED, {"scroll_speed", {}, {}, 0}},
    {SettingItem::GROUP_SELECTED_INDEX, {"group_selected_index", {}, {}, 0}},
    {SettingItem::UP_FILTER, {"up_filter", {}, {}, 0}},
    {SettingItem::M3U8_URL_ITEM, {"m3u8_url", {}, {}, 0}},
};

ProgramConfig::ProgramConfig() = default;

ProgramConfig::ProgramConfig(const ProgramConfig& conf) {
    this->setting = conf.setting;
    this->device  = conf.device;
    this->client  = conf.client;
}

void ProgramConfig::setProgramConfig(const ProgramConfig& conf) {
    this->setting = conf.setting;
    this->client  = conf.client;
    this->device  = conf.device;
    brls::Logger::info("setting: {}", conf.setting.dump());
}

std::string ProgramConfig::getClientID() {
    if (this->client.empty()) {
        this->client = fmt::format("{}.{}", tsvitch::getRandomNumber(), tsvitch::getUnixTime());
        this->save();
    }
    return this->client;
}

std::string ProgramConfig::getDeviceID() {
    return this->device;
}

void ProgramConfig::setDeviceID(const std::string& deviceId) {
    this->device = deviceId;
    this->save();
}

void ProgramConfig::loadHomeWindowState() {
    std::string homeWindowStateData = getSettingItem(SettingItem::HOME_WINDOW_STATE, std::string{""});

    if (homeWindowStateData.empty()) return;

    uint32_t hWidth, hHeight;
    int hXPos, hYPos;
    int monitor;

    sscanf(homeWindowStateData.c_str(), "%d,%ux%u,%dx%d", &monitor, &hWidth, &hHeight, &hXPos, &hYPos);

    if (hWidth == 0 || hHeight == 0) return;

    uint32_t minWidth  = getIntOption(SettingItem::MINIMUM_WINDOW_WIDTH);
    uint32_t minHeight = getIntOption(SettingItem::MINIMUM_WINDOW_HEIGHT);
    if (hWidth < minWidth) hWidth = minWidth;
    if (hHeight < minHeight) hHeight = minHeight;

    VideoContext::sizeH        = hHeight;
    VideoContext::sizeW        = hWidth;
    VideoContext::posX         = (float)hXPos;
    VideoContext::posY         = (float)hYPos;
    VideoContext::monitorIndex = monitor;

    brls::Logger::info("Load window state: {}x{},{}x{}", hWidth, hHeight, hXPos, hYPos);
}

void ProgramConfig::saveHomeWindowState() {
    if (std::isnan(VideoContext::posX) || std::isnan(VideoContext::posY)) return;
    auto videoContext = brls::Application::getPlatform()->getVideoContext();

    uint32_t width  = VideoContext::sizeW;
    uint32_t height = VideoContext::sizeH;
    int xPos        = VideoContext::posX;
    int yPos        = VideoContext::posY;

    int monitor = videoContext->getCurrentMonitorIndex();
    if (width == 0) width = brls::Application::ORIGINAL_WINDOW_WIDTH;
    if (height == 0) height = brls::Application::ORIGINAL_WINDOW_HEIGHT;
    brls::Logger::info("Save window state: {},{}x{},{}x{}", monitor, width, height, xPos, yPos);
    setSettingItem(SettingItem::HOME_WINDOW_STATE, fmt::format("{},{}x{},{}x{}", monitor, width, height, xPos, yPos));
}

void ProgramConfig::load() {
    const std::string path = this->getConfigDir() + "/tsvitch_config.json";

    std::ifstream readFile(path);
    if (readFile) {
        try {
            nlohmann::json content;
            readFile >> content;
            readFile.close();
            this->setProgramConfig(content.get<ProgramConfig>());
        } catch (const std::exception& e) {
            brls::Logger::error("ProgramConfig::load: {}", e.what());
        }
        brls::Logger::info("Load config from: {}", path);
    }

    this->m3u8Url = getSettingItem(SettingItem::M3U8_URL_ITEM, this->getM3U8Url());

#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    brls::DesktopPlatform::GAMEPAD_DB = getConfigDir() + "/gamecontrollerdb.txt";
#endif

    std::string customThemeID = getSettingItem(SettingItem::APP_RESOURCES, std::string{""});
    if (!customThemeID.empty()) {
        for (auto& theme : customThemes) {
            if (theme.id == customThemeID) {
                brls::View::CUSTOM_RESOURCES_PATH = theme.path;
                break;
            }
        }
        if (brls::View::CUSTOM_RESOURCES_PATH.empty()) {
            brls::Logger::warning("Custom theme not found: {}", customThemeID);
        }
    }

    std::string UIScale = getSettingItem(SettingItem::APP_UI_SCALE, std::string{""});
    if (UIScale == "544p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 960;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 544;
    } else if (UIScale == "720p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1280;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 720;
    } else if (UIScale == "900p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1600;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 900;
    } else if (UIScale == "1080p") {
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1920;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 1080;
    } else {
#ifdef __PSV__
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 960;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 544;
#else
        brls::Application::ORIGINAL_WINDOW_WIDTH  = 1280;
        brls::Application::ORIGINAL_WINDOW_HEIGHT = 720;
#endif
    }

    MPVCore::AUTO_PLAY = getBoolOption(SettingItem::PLAYER_AUTO_PLAY);

    MPVCore::VIDEO_SPEED = getIntOption(SettingItem::PLAYER_DEFAULT_SPEED);

    MPVCore::VIDEO_ASPECT = getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"});

    MPVCore::VIDEO_BRIGHTNESS = getSettingItem(SettingItem::PLAYER_BRIGHTNESS, 0);
    MPVCore::VIDEO_CONTRAST   = getSettingItem(SettingItem::PLAYER_CONTRAST, 0);
    MPVCore::VIDEO_SATURATION = getSettingItem(SettingItem::PLAYER_SATURATION, 0);
    MPVCore::VIDEO_HUE        = getSettingItem(SettingItem::PLAYER_HUE, 0);
    MPVCore::VIDEO_GAMMA      = getSettingItem(SettingItem::PLAYER_GAMMA, 0);

    VibrationHelper::GAMEPAD_VIBRATION = getBoolOption(SettingItem::GAMEPAD_VIBRATION);

    ImageHelper::REQUEST_THREADS = getIntOption(SettingItem::IMAGE_REQUEST_THREADS);

    brls::Application::setFPSStatus(!getBoolOption(SettingItem::HIDE_FPS));

    VideoContext::FULLSCREEN = getBoolOption(SettingItem::FULLSCREEN);

    VideoView::BOTTOM_BAR = getBoolOption(SettingItem::PLAYER_BOTTOM_BAR);

    VideoView::HIGHLIGHT_PROGRESS_BAR = getBoolOption(SettingItem::PLAYER_HIGHLIGHT_BAR);

#ifdef __PSV__
    MPVCore::HARDWARE_DEC = true;
#else
    MPVCore::HARDWARE_DEC = getBoolOption(SettingItem::PLAYER_HWDEC);
#endif

    MPVCore::PLAYER_HWDEC_METHOD = getSettingItem(SettingItem::PLAYER_HWDEC_CUSTOM, MPVCore::PLAYER_HWDEC_METHOD);

    VideoView::EXIT_FULLSCREEN_ON_END = getBoolOption(SettingItem::PLAYER_EXIT_FULLSCREEN_ON_END);

    MPVCore::INMEMORY_CACHE = getIntOption(SettingItem::PLAYER_INMEMORY_CACHE);

    brls::Label::OPENCC_ON = getBoolOption(SettingItem::OPENCC_ON);

    MPVCore::LOW_QUALITY = getBoolOption(SettingItem::PLAYER_LOW_QUALITY);

#ifdef _WIN32
    int scrollSpeed = getSettingItem(SettingItem::SCROLL_SPEED, 150);
#else
    int scrollSpeed = getSettingItem(SettingItem::SCROLL_SPEED, 100);
#endif
    brls::PanGestureRecognizer::panFactor = scrollSpeed * 0.01f;

    std::set<std::string> i18nData{
        brls::LOCALE_AUTO,    brls::LOCALE_EN_US,   brls::LOCALE_JA, brls::LOCALE_RYU,
        brls::LOCALE_ZH_HANS, brls::LOCALE_ZH_HANT, brls::LOCALE_Ko, brls::LOCALE_IT,
    };
    std::string langData = getSettingItem(SettingItem::APP_LANG, brls::LOCALE_AUTO);

    if (langData != brls::LOCALE_AUTO && i18nData.count(langData)) {
        brls::Platform::APP_LOCALE_DEFAULT = langData;
    } else {
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
        brls::Platform::APP_LOCALE_DEFAULT = brls::LOCALE_IT;
#endif
    }
#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)

    loadHomeWindowState();
#endif

    brls::Application::setLimitedFPS(getSettingItem(SettingItem::LIMITED_FPS, 0));

    int deactivatedTime = getSettingItem(SettingItem::DEACTIVATED_TIME, 0);
    if (deactivatedTime > 0) {
        brls::Application::setAutomaticDeactivation(true);
        brls::Application::setDeactivatedTime(deactivatedTime);
    }

    brls::Application::setDeactivatedFPS(getSettingItem(SettingItem::DEACTIVATED_FPS, 5));

    brls::Application::getWindowCreationDoneEvent()->subscribe([this]() {
        if (getBoolOption(SettingItem::APP_SWAP_ABXY)) {
            brls::Application::setSwapInputKeys(!brls::Application::isSwapInputKeys());
        }

        std::string themeData = getSettingItem(SettingItem::APP_THEME, std::string{"auto"});
        if (themeData == "light") {
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::LIGHT);
        } else if (themeData == "dark") {
            brls::Application::getPlatform()->setThemeVariant(brls::ThemeVariant::DARK);
        }

#if defined(__PSV__) || defined(PS4)
        brls::TextureCache::instance().cache.setCapacity(1);
#else
        brls::TextureCache::instance().cache.setCapacity(getSettingItem(SettingItem::TEXTURE_CACHE_NUM, 200));
#endif

        MPVCore::VIDEO_VOLUME = getSettingItem(SettingItem::PLAYER_VOLUME, 100);

#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
        int minWidth  = getIntOption(SettingItem::MINIMUM_WINDOW_WIDTH);
        int minHeight = getIntOption(SettingItem::MINIMUM_WINDOW_HEIGHT);
        brls::Application::getPlatform()->setWindowSizeLimits(minWidth, minHeight, 0, 0);
        checkOnTop();
#endif

        brls::Application::getPlatform()->getInputManager()->getKeyboardKeyStateChanged()->subscribe(
            [](brls::KeyState state) {
                if (!state.pressed) return;
                switch (state.key) {
#ifndef __APPLE__
                    case brls::BRLS_KBD_KEY_F11:
                        ProgramConfig::instance().toggleFullscreen();
                        break;
#endif
                    case brls::BRLS_KBD_KEY_F: {
                        auto activityStack  = brls::Application::getActivitiesStack();
                        brls::Activity* top = activityStack[activityStack.size() - 1];
                        if (!dynamic_cast<brls::EditTextDialog*>(top->getContentView())) {
                            ProgramConfig::instance().toggleFullscreen();
                        }
                        break;
                    }
                    case brls::BRLS_KBD_KEY_SPACE: {
                        auto activityStack  = brls::Application::getActivitiesStack();
                        brls::Activity* top = activityStack[activityStack.size() - 1];
                        VideoView* video    = dynamic_cast<VideoView*>(top->getContentView()->getView("video"));
                        if (video) {
                            video->togglePlay();
                        }
                        break;
                    }
                    default:
                        break;
                }
            });
    });

#ifdef IOS
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)

    brls::Application::getExitEvent()->subscribe([this]() { saveHomeWindowState(); });
#endif
}

ProgramOption ProgramConfig::getOptionData(SettingItem item) { return SETTING_MAP[item]; }

size_t ProgramConfig::getIntOptionIndex(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            int option = this->setting.at(optionData.key).get<int>();
            for (size_t i = 0; i < optionData.rawOptionList.size(); i++) {
                if (optionData.rawOptionList[i] == option) return i;
            }
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

int ProgramConfig::getIntOption(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            return this->setting.at(optionData.key).get<int>();
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.rawOptionList[optionData.defaultOption];
        }
    }
    return optionData.rawOptionList[optionData.defaultOption];
}

bool ProgramConfig::getBoolOption(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            return this->setting.at(optionData.key).get<bool>();
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

int ProgramConfig::getStringOptionIndex(SettingItem item) {
    auto optionData = getOptionData(item);
    if (setting.contains(optionData.key)) {
        try {
            std::string option = this->setting.at(optionData.key).get<std::string>();
            for (size_t i = 0; i < optionData.optionList.size(); ++i)
                if (optionData.optionList[i] == option) return i;
        } catch (const std::exception& e) {
            brls::Logger::error("Damaged config found: {}/{}", optionData.key, e.what());
            return optionData.defaultOption;
        }
    }
    return optionData.defaultOption;
}

void ProgramConfig::save() {
    const std::string path = this->getConfigDir() + "/tsvitch_config.json";

#ifndef IOS
    cpr::fs::create_directories(this->getConfigDir());
#endif
    nlohmann::json content(*this);
    std::ofstream writeFile(path);
    if (!writeFile) {
        brls::Logger::error("Cannot write config to: {}", path);
        return;
    }
    writeFile << content.dump(2);
    writeFile.close();
    brls::Logger::info("Write config to: {}", path);
}

void ProgramConfig::checkOnTop() {
    switch (getIntOption(SettingItem::ON_TOP_MODE)) {
        case 0:

            brls::Application::getPlatform()->setWindowAlwaysOnTop(false);
            return;
        case 1:

            brls::Application::getPlatform()->setWindowAlwaysOnTop(true);
            return;
        case 2: {
            double factor     = brls::Application::getPlatform()->getVideoContext()->getScaleFactor();
            uint32_t minWidth = ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_WINDOW_WIDTH) * factor + 0.1;
            uint32_t minHeight =
                ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_WINDOW_HEIGHT) * factor + 0.1;
            bool onTop = brls::Application::windowWidth <= minWidth || brls::Application::windowHeight <= minHeight;
            brls::Application::getPlatform()->setWindowAlwaysOnTop(onTop);
            break;
        }
        default:
            break;
    }
}

void ProgramConfig::init() {
    brls::Logger::info("tsvitch {}", APPVersion::instance().git_tag);
    tsvitch::initCrashDump();

    brls::Application::getWindowSizeChangedEvent()->subscribe([]() { ProgramConfig::instance().checkOnTop(); });

    curl_global_init(CURL_GLOBAL_DEFAULT);
    cpr::async::startup(THREAD_POOL_MIN_THREAD_NUM, THREAD_POOL_MAX_THREAD_NUM, std::chrono::milliseconds(5000));

#ifdef _WIN32
    WSADATA wsaData;
    int result = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (result != 0) brls::Logger::error("WSAStartup failed with error: {}", result);
#endif
#if defined(_MSC_VER)
#elif defined(__PSV__)
#elif defined(PS4)
    if (sceSysmoduleLoadModuleInternal(ORBIS_SYSMODULE_INTERNAL_NET) < 0) brls::Logger::error("cannot load net module");
    primary_dns                     = inet_addr(primaryDNSStr.c_str());
    secondary_dns                   = inet_addr(secondaryDNSStr.c_str());
    ps4_mpv_use_precompiled_shaders = 1;
    ps4_mpv_dump_shaders            = 0;

    brls::sync([]() { sceSystemServiceHideSplashScreen(); });
#else
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        brls::Logger::info("Current working directory: {}", cwd);
    }
#endif

    this->loadCustomThemes();

    this->load();

    brls::FontLoader::USER_FONT_PATH = getConfigDir() + "/font.ttf";
    brls::FontLoader::USER_ICON_PATH = getConfigDir() + "/icon.ttf";

    if (access(brls::FontLoader::USER_ICON_PATH.c_str(), F_OK) == -1) {
#if defined(__PSV__) || defined(PS4)
        brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
#else
        std::string icon = getSettingItem(SettingItem::KEYMAP, std::string{"xbox"});
        if (icon == "xbox") {
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_xbox.ttf");
        } else if (icon == "ps") {
            brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_ps.ttf");
        } else {
            if (getBoolOption(SettingItem::APP_SWAP_ABXY)) {
                brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard_swap.ttf");
            } else {
                brls::FontLoader::USER_ICON_PATH = BRLS_ASSET("font/keymap_keyboard.ttf");
            }
        }
#endif
    }

    brls::FontLoader::USER_EMOJI_PATH = getConfigDir() + "/emoji.ttf";
    if (access(brls::FontLoader::USER_EMOJI_PATH.c_str(), F_OK) == -1) {
        brls::FontLoader::USER_EMOJI_PATH = BRLS_ASSET("font/emoji.ttf");
    }
}

std::string ProgramConfig::getHomePath() {
#if defined(__SWITCH__)
    return "/";
#elif defined(_WIN32)
    return std::string(getenv("HOMEPATH"));
#else
    return std::string(getenv("HOME"));
#endif
}

std::string ProgramConfig::getConfigDir() {
#ifdef __SWITCH__
    return "/config/tsvitch";
#elif defined(PS4)
    return "/data/tsvitch";
#elif defined(__PSV__)
    return "ux0:/data/tsvitch";
#elif defined(IOS)
    CFURLRef homeURL = CFCopyHomeDirectoryURL();
    if (homeURL != nullptr) {
        char buffer[PATH_MAX];
        if (CFURLGetFileSystemRepresentation(homeURL, true, reinterpret_cast<UInt8*>(buffer), sizeof(buffer))) {
        }
        CFRelease(homeURL);
        return std::string{buffer} + "/Library/Preferences";
    }
    return "../Library/Preferences";
#else
#ifdef _DEBUG
    char currentPathBuffer[PATH_MAX];
    std::string currentPath = getcwd(currentPathBuffer, sizeof(currentPathBuffer));
#ifdef _WIN32
    return currentPath + "\\config\\tsvitch";
#else
    return currentPath + "/config/tsvitch";
#endif
#else
#ifdef __APPLE__
    return std::string(getenv("HOME")) + "/Library/Application Support/tsvitch";
#endif
#ifdef __linux__
    std::string config = "";
    char* config_home  = getenv("XDG_CONFIG_HOME");
    if (config_home) config = std::string(config_home);
    if (config.empty()) config = std::string(getenv("HOME")) + "/.config";
    return config + "/tsvitch";
#endif
#ifdef _WIN32
    WCHAR wpath[MAX_PATH];
    std::vector<char> lpath(MAX_PATH);
    SHGetSpecialFolderPathW(0, wpath, CSIDL_LOCAL_APPDATA, false);
    WideCharToMultiByte(CP_UTF8, 0, wpath, std::wcslen(wpath), lpath.data(), lpath.size(), nullptr, nullptr);
    return std::string(lpath.data()) + "\\giovannimirulla\\tsvitch";
#endif
#endif
#endif
}

void ProgramConfig::exit(char* argv[]) {
    cpr::async::cleanup();
    curl_global_cleanup();

#ifdef _WIN32
    WSACleanup();
#endif
#ifdef IOS
#elif defined(PS4)
#elif __PSV__
#elif defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
    if (!brls::DesktopPlatform::RESTART_APP) return;
#ifdef __linux__
    char filePath[PATH_MAX + 1];
    ssize_t count = readlink("/proc/self/exe", filePath, PATH_MAX);
    if (count <= 0)
        strcpy(filePath, argv[0]);
    else
        filePath[count] = 0;
#else
    char* filePath = argv[0];
#endif

    brls::Logger::info("Restart app {}", filePath);

    execv(filePath, argv);
#endif
}

void ProgramConfig::loadCustomThemes() {
    customThemes.clear();
    std::string directoryPath = getConfigDir() + "/theme";
    if (!cpr::fs::exists(directoryPath)) return;

    for (const auto& entry : cpr::fs::directory_iterator(getConfigDir() + "/theme")) {
#if USE_BOOST_FILESYSTEM
        if (!cpr::fs::is_directory(entry)) continue;
#else
        if (!entry.is_directory()) continue;
#endif
        std::string subDirectory = entry.path().string();
        std::string jsonFilePath = subDirectory + "/resources_meta.json";
        if (!cpr::fs::exists(jsonFilePath)) continue;

        std::ifstream readFile(jsonFilePath);
        if (readFile) {
            try {
                nlohmann::json content;
                readFile >> content;
                readFile.close();
                CustomTheme customTheme;
                customTheme.path = subDirectory + "/";
                customTheme.id   = entry.path().filename().string();
                content.get_to(customTheme);
                customThemes.emplace_back(customTheme);
                brls::Logger::info("Load custom theme \"{}\" from: {}", customTheme.name, jsonFilePath);
            } catch (const std::exception& e) {
                brls::Logger::error("CustomTheme::load: {}", e.what());
                continue;
            }
        }
    }
}

std::vector<CustomTheme> ProgramConfig::getCustomThemes() { return customThemes; }

std::string ProgramConfig::getM3U8Url() {
    if (m3u8Url.empty())
        return M3U8_URL_VALUE;
    else
        return this->m3u8Url;
}
//https://raw.githubusercontent.com/Free-TV/IPTV/refs/heads/master/playlists/playlist_italy.m3u8
void ProgramConfig::setM3U8Url(const std::string& url) {
    this->m3u8Url = url;
    brls::Logger::info("setM3U8Url: {}", m3u8Url);
    setSettingItem(SettingItem::M3U8_URL_ITEM, m3u8Url);
    if (m3u8Url.empty()) {
        m3u8Url = M3U8_URL_VALUE;
    }
    GA("m3u8_url", {{"url", m3u8Url}});
}

void ProgramConfig::toggleFullscreen() {
    bool value = !getBoolOption(SettingItem::FULLSCREEN);
    setSettingItem(SettingItem::FULLSCREEN, value);
    VideoContext::FULLSCREEN = value;
    brls::Application::getPlatform()->getVideoContext()->fullScreen(value);
    GA("player_setting", {{"fullscreen", value ? "true" : "false"}});
}