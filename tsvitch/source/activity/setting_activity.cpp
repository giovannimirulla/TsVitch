#include <pystring.h>
#include <borealis/core/i18n.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/cache_helper.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/views/cells/cell_bool.hpp>
#include <borealis/views/cells/cell_input.hpp>
#include <cpr/cpr.h>

#include "tsvitch.h"
#include "activity/setting_activity.hpp"
#include "fragment/setting_network.hpp"
#include "fragment/test_rumble.hpp"
#include "utils/config_helper.hpp"
#include "utils/vibration_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "utils/activity_helper.hpp"
#include "view/text_box.hpp"
#include "view/selector_cell.hpp"
#include "view/mpv_core.hpp"

#if defined(__APPLE__) || defined(__linux__) || defined(_WIN32)
#include "borealis/platforms/desktop/desktop_platform.hpp"
#endif

#ifdef __linux__
#include "borealis/platforms/desktop/steam_deck.hpp"
#endif

using namespace brls::literals;

const std::map<std::string, std::map<std::string, std::string>> OPENSOURCE = {
    {"FFmpeg",
     {{"Official site", "https://www.ffmpeg.org"},
      {"Notes", "Copyright (c) FFmpeg developers and contributors.\nLicensed under LGPLv2.1 or later"}}},
    {"mpv",
     {{"Official site", "https://mpv.io"},
      {"Notes", "Copyright (c) mpv developers and contributors.\nLicensed under GPL-2.0 or LGPLv2.1"}}},
    {"borealis",
     {{"Official site", "https://github.com/xfangfang/borealis"},
      {"Notes",
       "Copyright (c) 2019-2022, natinusala and contributors.\nCopyright (c) xfangfang.\nLicensed under Apache-2.0 "
       "license"}}},
    {"OpenCC",
     {{"Official site", "https://github.com/xfangfang/OpenCC"},
      {"Notes", "Copyright (c) Carbo Kuo and contributors.\nLicensed under Apache-2.0 license"}}},
    {"pystring",
     {{"Official site", "https://github.com/imageworks/pystring"},
      {"Notes", "Copyright (c) imageworks and contributors.\nLicensed under BCD-3-Clause license"}}},
    {"QR-Code-generator",
     {{"Official site", "https://www.nayuki.io/page/qr-code-generator-library"},
      {"GitHub", "https://github.com/nayuki/QR-Code-generator"},
      {"Notes", "Copyright © 2020 Project Nayuki.\nLicensed under MIT license"}}},
    {"lunasvg",
     {{"Official site", "https://github.com/sammycage/lunasvg"},
      {"Notes", "Copyright (c) 2020 Nwutobo Samuel Ugochukwu.\nLicensed under MIT license"}}},
    {"cpr",
     {{"Official site", "https://docs.libcpr.org"},
      {"GitHub", "https://github.com/libcpr/cpr"},
      {"Notes",
       "Copyright (c) 2017-2021 Huu Nguyen.\nCopyright (c) 2022 libcpr and many other contributors.\nLicensed under "
       "MIT license"}}},
#ifdef USE_WEBP
    {"libwebp",
     {{"Official site", "https://chromium.googlesource.com/webm/libwebp"},
      {"Notes",
       "Copyright (c) Google Inc. All Rights Reserved.\nLicensed under BSD 3-Clause \"New\" or \"Revised\" License"}}},
#endif
#ifdef __SWITCH__
    {"nx",
     {{"Official site", "https://github.com/switchbrew/libnx"},
      {"Notes", "Copyright 2017-2018 libnx Authors.\nPublic domain"}}},
    {"devkitPro",
     {{"Official site", "https://devkitpro.org"}, {"Notes", "Copyright devkitPro Authors.\nPublic domain"}}},
#endif
#ifdef __PSV__
    {"vitasdk",
     {{"Official site", "https://github.com/vitasdk"}, {"Notes", "Copyright vitasdk Authors.\nPublic domain"}}},
#endif
#ifdef PS4
    {"pacbrew",
     {{"Official site", "https://github.com/PacBrew/pacbrew-packages"},
      {"Notes", "Copyright PacBrew Authors.\nPublic domain"}}},
    {"OpenOrbis-PS4-Toolchain",
     {{"Official site", "https://github.com/OpenOrbis/OpenOrbis-PS4-Toolchain"},
      {"Notes", "Copyright OpenOrbis Authors.\nLicensed under GPL-3.0"}}},
#endif
};

SettingActivity::SettingActivity(std::function<void()> onClose) : onCloseCallback(onClose) {
    brls::Logger::debug("SettingActivity: create");
    GA("open_setting")
}

void SettingActivity::onContentAvailable() {
    brls::Logger::debug("SettingActivity: onContentAvailable");

#ifdef __SWITCH__
    btnTutorialOpenApp->registerClickAction([](...) -> bool {
        Intent::openHint();
        return true;
    });
#else
    btnTutorialOpenApp->setVisibility(brls::Visibility::GONE);
#endif

#ifdef __SWITCH__
    btnTutorialError->registerClickAction([](...) -> bool {
        auto dialog =
            new brls::Dialog((brls::Box*)brls::View::createFromXMLResource("fragment/settings_tutorial_error.xml"));
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return true;
    });
#else
    btnTutorialError->setVisibility(brls::Visibility::GONE);
#endif

#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
    btnOpenConfig->title->setText("tsvitch/setting/tools/others/config_dir"_i18n);
#endif
#ifdef __linux__
    if (brls::isSteamDeck()) {
        btnOpenConfig->title->setText("tsvitch/setting/tools/others/config_dir"_i18n);
    }
#endif
    btnOpenConfig->registerClickAction([](...) -> bool {
        auto configPath = ProgramConfig::instance().getConfigDir();
        brls::Application::notify("tsvitch/setting/tools/others/config_dir"_i18n + ": " + configPath);
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
#ifdef __linux__
        if (!brls::isSteamDeck())
#endif
        {
            auto* p = (brls::DesktopPlatform*)brls::Application::getPlatform();
            p->openBrowser(configPath);
        }
#endif
        return true;
    });

    btnTutorialFont->registerClickAction([](...) -> bool {
        auto dialog =
            new brls::Dialog((brls::Box*)brls::View::createFromXMLResource("fragment/settings_tutorial_font.xml"));
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return true;
    });

    btnNetworkChecker->registerClickAction([](...) -> bool {
        auto dialog = new brls::Dialog((brls::Box*)new SettingNetwork());
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return true;
    });

    btnProxyTest->registerClickAction([](...) -> bool {
        // Testa o proxy fazendo uma requisição simples
        std::string proxyUrl = ProgramConfig::instance().getProxyUrl();
        
        if (proxyUrl.empty()) {
            brls::Application::notify("tsvitch/setting/tools/test/proxy_none"_i18n);
            return true;
        }
        
        brls::Application::notify("tsvitch/setting/tools/test/proxy_testing"_i18n + ": " + proxyUrl);
        
        // Faz o teste usando a configuração atual do sistema
        try {
            // Usa a configuração de proxy atual que já foi aplicada ao sistema
            auto response = cpr::Get(cpr::Url{"http://httpbin.org/ip"}, 
                                   cpr::Timeout{5000});
            
            if (response.status_code == 200) {
                brls::Application::notify("tsvitch/setting/tools/test/proxy_success"_i18n);
            } else if (response.status_code == 0) {
                brls::Application::notify("tsvitch/setting/tools/test/proxy_error"_i18n + ": " + response.error.message);
            } else {
                brls::Application::notify("tsvitch/setting/tools/test/proxy_failed"_i18n + ": " + std::to_string(response.status_code));
            }
        } catch (const std::exception& e) {
            brls::Application::notify("tsvitch/setting/tools/test/proxy_error"_i18n + ": " + std::string(e.what()));
        }
        
        return true;
    });

#ifdef __SWITCH__
    btnVibrationTest->registerClickAction([](...) -> bool {
        auto dialog = new brls::Dialog((brls::Box*)new TestRumble());
        dialog->addButton("hints/ok"_i18n, []() {});
        dialog->open();
        return true;
    });
#else
    btnVibrationTest->setVisibility(brls::Visibility::GONE);
#endif

    std::string version = APPVersion::instance().git_tag.empty() ? "v" + APPVersion::instance().getVersionStr()
                                                                 : APPVersion::instance().git_tag;
    btnReleaseChecker->title->setText("tsvitch/setting/tools/others/release"_i18n + " (" + "hints/current"_i18n + ": " +
                                      version + ")");
    btnReleaseChecker->registerClickAction([](...) -> bool {
        brls::Application::notify("tsvitch/setting/tools/others/checking_update"_i18n);
        APPVersion::instance().checkUpdate(0, true);
        return true;
    });

    labelAboutVersion->setText(version
#if defined(BOREALIS_USE_DEKO3D)
                               + " (deko3d)"
#elif defined(BOREALIS_USE_OPENGL)
#if defined(USE_GL2)
                               + " (OpenGL2)"
#elif defined(USE_GLES2)
                               + " (OpenGL ES2)"
#elif defined(USE_GLES3)
                               + " (OpenGL ES3)"
#else
                               + " (OpenGL)"
#endif
#elif defined(BOREALIS_USE_D3D11)
                               + " (D3D11)"
#endif
    );

    //for every key in OPENSOURCE add this:
    //  <brls:Header
    //             title="@i18n/tsvitch/setting/about/brief_header"
    //             marginBottom="@style/tsvitch/margin/20"/>

    //     <brls:Label
    //             marginLeft="20"
    //             marginBottom="@style/tsvitch/margin/20"
    //             text="@i18n/tsvitch/setting/about/brief"/>
    //     <brls:Header
    //             title="@i18n/tsvitch/setting/about/repo_header"
    //             marginBottom="@style/tsvitch/margin/20"/>
    //     <brls:Label
    //             textColor="#6693B6"
    //             marginLeft="20"
    //             marginBottom="@style/tsvitch/margin/20"
    //             text="@i18n/tsvitch/github"/>
    // in box

    for (const auto& [name, data] : OPENSOURCE) {
        auto* header = new brls::Header();
        header->setTitle(name);
        header->setMarginBottom(20);
        this->boxOpensource->addView(header);

        for (const auto& [key, value] : data) {
            auto* label = new brls::Label();
            std::string text = key + ": " + value;
            label->setText(text);
            label->setMarginLeft(20);
            label->setMarginBottom(20);
             this->boxOpensource->addView(label);
            
        }
    }

#ifdef IOS
    btnQuit->setVisibility(brls::Visibility::GONE);
#else
    btnQuit->registerClickAction([](...) -> bool {
        auto dialog = new brls::Dialog("hints/exit_hint"_i18n);
        dialog->addButton("hints/cancel"_i18n, []() {});
        dialog->addButton("hints/ok"_i18n, []() { brls::Application::quit(); });
        dialog->open();
        return true;
    });
#endif

    auto& conf = ProgramConfig::instance();

    cellShowBar->init("tsvitch/setting/app/others/show_bottom"_i18n, !conf.getBoolOption(SettingItem::HIDE_BOTTOM_BAR),
                      [](bool value) {
                          value = !value;
                          ProgramConfig::instance().setSettingItem(SettingItem::HIDE_BOTTOM_BAR, value);

                          brls::AppletFrame::HIDE_BOTTOM_BAR = value;

                          auto stack = brls::Application::getActivitiesStack();
                          for (auto& activity : stack) {
                              auto* frame = dynamic_cast<brls::AppletFrame*>(activity->getContentView());
                              if (!frame) continue;
                              frame->setFooterVisibility(value ? brls::Visibility::GONE : brls::Visibility::VISIBLE);
                          }
                      });

    cellShowFPS->init("tsvitch/setting/app/others/show_fps"_i18n, !conf.getBoolOption(SettingItem::HIDE_FPS),
                      [](bool value) {
                          ProgramConfig::instance().setSettingItem(SettingItem::HIDE_FPS, !value);
                          brls::Application::setFPSStatus(value);
                      });

    auto fpsOption = conf.getOptionData(SettingItem::LIMITED_FPS);
    selectorFPS->init("tsvitch/setting/app/others/limited_fps"_i18n,
                      {"tsvitch/setting/app/others/limited_fps_vsync"_i18n, "30", "60", "90", "120"},
                      (size_t)conf.getIntOptionIndex(SettingItem::LIMITED_FPS), [fpsOption](int data) {
                          int fps = fpsOption.rawOptionList[data];
                          brls::Application::setLimitedFPS(fps);
                          ProgramConfig::instance().setSettingItem(SettingItem::LIMITED_FPS, fps);
                          return true;
                      });

#ifdef __SWITCH__
    cellVibration->init("tsvitch/setting/app/others/vibration"_i18n, conf.getBoolOption(SettingItem::GAMEPAD_VIBRATION),
                        [](bool value) {
                            ProgramConfig::instance().setSettingItem(SettingItem::GAMEPAD_VIBRATION, value);
                            VibrationHelper::GAMEPAD_VIBRATION = value;
                        });
#else
    cellVibration->setVisibility(brls::Visibility::GONE);
#endif

#ifdef ALLOW_FULLSCREEN
    cellFullscreen->init("tsvitch/setting/app/others/fullscreen"_i18n, conf.getBoolOption(SettingItem::FULLSCREEN),
                         [](bool value) {
                             ProgramConfig::instance().setSettingItem(SettingItem::FULLSCREEN, value);
                             // 更新设置
                             VideoContext::FULLSCREEN = value;
                             // 设置当前状态
                             brls::Application::getPlatform()->getVideoContext()->fullScreen(value);
                         });

    auto setOnTopCell = [this](bool enabled) {
        if (enabled) {
            cellOnTopMode->setDetailTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);
        } else {
            cellOnTopMode->setDetailTextColor(brls::Application::getTheme()["brls/text_disabled"]);
        }
    };
    setOnTopCell(conf.getIntOptionIndex(SettingItem::ON_TOP_MODE) != 0);
    int onTopModeIndex = conf.getIntOption(SettingItem::ON_TOP_MODE);
    cellOnTopMode->setText("tsvitch/setting/app/others/always_on_top"_i18n);
    std::vector<std::string> onTopOptionList = {"hints/off"_i18n, "hints/on"_i18n,
                                                "tsvitch/player/setting/aspect/auto"_i18n};
    cellOnTopMode->setDetailText(onTopOptionList[onTopModeIndex]);
    cellOnTopMode->registerClickAction([this, onTopOptionList, setOnTopCell](brls::View* view) {
        BaseDropdown::text(
            "tsvitch/setting/app/others/always_on_top"_i18n, onTopOptionList,
            [this, onTopOptionList, setOnTopCell](int data) {
                cellOnTopMode->setDetailText(onTopOptionList[data]);
                ProgramConfig::instance().setSettingItem(SettingItem::ON_TOP_MODE, data);
                ProgramConfig::instance().checkOnTop();
                setOnTopCell(data != 0);
            },
            ProgramConfig::instance().getIntOption(SettingItem::ON_TOP_MODE),
            "tsvitch/setting/app/others/always_on_top_hint"_i18n);
        return true;
    });

#else
    cellFullscreen->setVisibility(brls::Visibility::GONE);
    cellOnTopMode->setVisibility(brls::Visibility::GONE);
#endif

    static int themeData = conf.getStringOptionIndex(SettingItem::APP_THEME);
    selectorTheme->init("tsvitch/setting/app/others/theme/header"_i18n,
                        {"tsvitch/setting/app/others/theme/1"_i18n, "tsvitch/setting/app/others/theme/2"_i18n,
                         "tsvitch/setting/app/others/theme/3"_i18n},
                        themeData, [](int data) {
                            if (themeData == data) return false;
                            themeData       = data;
                            auto optionData = ProgramConfig::instance().getOptionData(SettingItem::APP_THEME);
                            ProgramConfig::instance().setSettingItem(SettingItem::APP_THEME,
                                                                     optionData.optionList[data]);
                            DialogHelper::quitApp();
                            return true;
                        });

    std::string customThemeID = conf.getSettingItem(SettingItem::APP_RESOURCES, std::string{""});
    conf.loadCustomThemes();
    auto customThemeList = conf.getCustomThemes();
    if (customThemeList.empty()) {
        selectorCustomTheme->setVisibility(brls::Visibility::GONE);
    } else {
        std::vector<std::string> customThemeNameList = {"hints/off"_i18n};
        int customThemeIndex                         = 0;
        for (size_t index = 0; index < customThemeList.size(); index++) {
            customThemeNameList.emplace_back(customThemeList[index].name);
            if (customThemeID == customThemeList[index].id) {
                customThemeIndex = index + 1;
            }
        }
        selectorCustomTheme->init("tsvitch/setting/app/others/custom_theme/header"_i18n, customThemeNameList,
                                  customThemeIndex, [customThemeIndex, customThemeList](int data) {
                                      if (customThemeIndex == data) return false;
                                      if (data <= 0) {
                                          ProgramConfig::instance().setSettingItem(SettingItem::APP_RESOURCES, "");
                                      } else {
                                          ProgramConfig::instance().setSettingItem(SettingItem::APP_RESOURCES,
                                                                                   customThemeList[data - 1].id);
                                      }

                                      DialogHelper::quitApp();
                                      return true;
                                  });
    }

    static int UIScaleIndex = conf.getStringOptionIndex(SettingItem::APP_UI_SCALE);
    selectorUIScale->init("tsvitch/setting/app/others/scale/header"_i18n,
                          {
                              "tsvitch/setting/app/others/scale/544p"_i18n,
                              "tsvitch/setting/app/others/scale/720p"_i18n,
                              "tsvitch/setting/app/others/scale/900p"_i18n,
                              "tsvitch/setting/app/others/scale/1080p"_i18n,
                          },
                          UIScaleIndex, [](int data) {
                              if (UIScaleIndex == data) return false;
                              UIScaleIndex    = data;
                              auto optionData = ProgramConfig::instance().getOptionData(SettingItem::APP_UI_SCALE);
                              ProgramConfig::instance().setSettingItem(SettingItem::APP_UI_SCALE,
                                                                       optionData.optionList[data]);
                              DialogHelper::quitApp();
                              return true;
                          });

#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
    static int keyIndex = conf.getStringOptionIndex(SettingItem::KEYMAP);
    selectorKeymap->init("tsvitch/setting/app/others/keymap/header"_i18n,
                         {
                             "tsvitch/setting/app/others/keymap/xbox"_i18n,
                             "tsvitch/setting/app/others/keymap/ps"_i18n,
                             "tsvitch/setting/app/others/keymap/keyboard"_i18n,
                         },
                         keyIndex, [](int data) {
                             if (keyIndex == data) return false;
                             keyIndex        = data;
                             auto optionData = ProgramConfig::instance().getOptionData(SettingItem::KEYMAP);
                             ProgramConfig::instance().setSettingItem(SettingItem::KEYMAP, optionData.optionList[data]);
                             DialogHelper::quitApp();
                             return true;
                         });
#else
    selectorKeymap->setVisibility(brls::Visibility::GONE);
#endif

    btnKeymapSwap->init("tsvitch/setting/app/others/keymap/swap"_i18n, conf.getBoolOption(SettingItem::APP_SWAP_ABXY),
                        [](bool data) {
                            ProgramConfig::instance().setSettingItem(SettingItem::APP_SWAP_ABXY, data);
                            DialogHelper::quitApp();
                        });

    static int langIndex = conf.getStringOptionIndex(SettingItem::APP_LANG);
    selectorLang->init(
        "tsvitch/setting/app/others/language/header"_i18n,
        {
#if defined(__SWITCH__) || defined(__PSV__) || defined(PS4)
            "tsvitch/setting/app/others/language/auto"_i18n,
#endif
            "tsvitch/setting/app/others/language/english"_i18n, "tsvitch/setting/app/others/language/japanese"_i18n,
            "tsvitch/setting/app/others/language/ryukyuan"_i18n, "tsvitch/setting/app/others/language/chinese_t"_i18n,
            "tsvitch/setting/app/others/language/chinese_s"_i18n, "tsvitch/setting/app/others/language/korean"_i18n,
            "tsvitch/setting/app/others/language/italiano"_i18n,
            "tsvitch/setting/app/others/language/portuguese_br"_i18n},
        langIndex, [](int data) {
            if (langIndex == data) return false;
            langIndex       = data;
            auto optionData = ProgramConfig::instance().getOptionData(SettingItem::APP_LANG);
            ProgramConfig::instance().setSettingItem(SettingItem::APP_LANG, optionData.optionList[data]);
            DialogHelper::quitApp();
            return true;
        });

#if defined(IOS) || defined(DISABLE_OPENCC)
    btnOpencc->setVisibility(brls::Visibility::GONE);
#else
    if (brls::Application::getLocale() == brls::LOCALE_ZH_HANT ||
        brls::Application::getLocale() == brls::LOCALE_ZH_TW) {
        btnOpencc->init("tsvitch/setting/app/others/opencc"_i18n, conf.getBoolOption(SettingItem::OPENCC_ON),
                        [](bool value) {
                            ProgramConfig::instance().setSettingItem(SettingItem::OPENCC_ON, value);
                            DialogHelper::quitApp();
                        });
    } else {
        btnOpencc->setVisibility(brls::Visibility::GONE);
    }
#endif

#if defined(__PSV__) || defined(PS4)
    selectorTexture->setVisibility(brls::Visibility::GONE);
#else
    selectorTexture->init("tsvitch/setting/app/image/texture"_i18n,
                          {"100", "200 (" + "hints/preset"_i18n + ")", "300", "400", "500"},
                          conf.getSettingItem(SettingItem::TEXTURE_CACHE_NUM, 200) / 100 - 1, [](int data) {
                              int num = 100 * data + 100;
                              ProgramConfig::instance().setSettingItem(SettingItem::TEXTURE_CACHE_NUM, num);
                              brls::TextureCache::instance().cache.setCapacity(num);
                          });
#endif

    auto threadOption = conf.getOptionData(SettingItem::IMAGE_REQUEST_THREADS);
    selectorThreads->init("tsvitch/setting/app/image/threads"_i18n, threadOption.optionList,
                          conf.getIntOptionIndex(SettingItem::IMAGE_REQUEST_THREADS), [threadOption](int data) {
                              ProgramConfig::instance().setSettingItem(SettingItem::IMAGE_REQUEST_THREADS,
                                                                       threadOption.rawOptionList[data]);
                              ImageHelper::setRequestThreads(threadOption.rawOptionList[data]);
                          });

    selectorInmemory->init("tsvitch/setting/app/playback/in_memory_cache"_i18n,
#ifdef __PSV__
                           {"0MB (" + "hints/off"_i18n + ")", "1MB", "5MB", "10MB"},
#else
        {"0MB (" + "hints/off"_i18n + ")", "10MB", "20MB", "50MB", "100MB"},
#endif
                           conf.getIntOptionIndex(SettingItem::PLAYER_INMEMORY_CACHE), [](int data) {
                               auto inmemoryOption =
                                   ProgramConfig::instance().getOptionData(SettingItem::PLAYER_INMEMORY_CACHE);
                               ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_INMEMORY_CACHE,
                                                                        inmemoryOption.rawOptionList[data]);
                               if (MPVCore::INMEMORY_CACHE == inmemoryOption.rawOptionList[data]) return;
                               MPVCore::INMEMORY_CACHE = inmemoryOption.rawOptionList[data];
                               MPVCore::instance().restart();
                           });

    auto m3u8Url = conf.getSettingItem(SettingItem::M3U8_URL_ITEM, std::string{""});
    btnM3U8Input->init(
        "tsvitch/setting/tools/m3u8/input"_i18n, m3u8Url,
        [](const std::string& data) {
            std::string m3u8Url = pystring::strip(data);
            ProgramConfig::instance().setM3U8Url(m3u8Url);
            OnM3U8UrlChanged.fire(); // Notifica tutte le view interessate
        },
        "tsvitch/setting/tools/m3u8/hint"_i18n, "tsvitch/setting/tools/m3u8/hint"_i18n, 255);
    
    // Soluzione definitiva per l'overflow del testo nell'InputCell
    btnM3U8Input->detail->setMaxWidth(140);      // Riduciamo a 140px per essere sicuri
    btnM3U8Input->detail->setSingleLine(true);   // Forza una sola linea
    btnM3U8Input->detail->setEllipsisWidth(20);  // Imposta la larghezza per l'ellissi
    btnM3U8Input->detail->setRequiredWidth(140); // Forza anche la required width
    
    // Applica troncamento manuale del testo se necessario
    auto currentText = btnM3U8Input->detail->getFullText();
    if (currentText.length() > 30) {
        // Mostra inizio e fine dell'URL con ellissi al centro
        std::string truncated = currentText.substr(0, 15) + "..." + currentText.substr(currentText.length() - 12);
        btnM3U8Input->detail->setText(truncated);
    }

    // Inizializza il selector per il timeout M3U8
    auto timeoutOption = ProgramConfig::instance().getOptionData(SettingItem::M3U8_TIMEOUT);
    selectorM3U8Timeout->init("tsvitch/setting/tools/m3u8/timeout"_i18n,
                              timeoutOption.optionList,
                              timeoutOption.defaultOption,
                              [](int data) {
                                  auto timeoutOption = ProgramConfig::instance().getOptionData(SettingItem::M3U8_TIMEOUT);
                                  ProgramConfig::instance().setSettingItem(SettingItem::M3U8_TIMEOUT, timeoutOption.rawOptionList[data]);
                              });

    auto proxyUrl = conf.getSettingItem(SettingItem::PROXY_URL_ITEM, std::string{""});
    btnProxyInput->init(
        "tsvitch/setting/tools/proxy/input"_i18n, proxyUrl,
        [](const std::string& data) {
            std::string proxyUrl = pystring::strip(data);
            ProgramConfig::instance().setProxyUrl(proxyUrl);
        },
        "tsvitch/setting/tools/proxy/hint"_i18n, "tsvitch/setting/tools/proxy/hint"_i18n, 255);

#if defined(PS4) || defined(__PSV__)
    btnHWDEC->setVisibility(brls::Visibility::GONE);
#else
    btnHWDEC->init("tsvitch/setting/app/playback/hwdec"_i18n, conf.getBoolOption(SettingItem::PLAYER_HWDEC),
                   [](bool value) {
                       ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_HWDEC, value);
                       if (MPVCore::HARDWARE_DEC == value) return;
                       MPVCore::HARDWARE_DEC = value;
                       MPVCore::instance().restart();
                   });
#endif
    btnQuality->init("tsvitch/setting/app/playback/low_quality"_i18n,
                     conf.getBoolOption(SettingItem::PLAYER_LOW_QUALITY), [](bool value) {
                         ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_LOW_QUALITY, value);
                         if (MPVCore::LOW_QUALITY == value) return;
                         MPVCore::LOW_QUALITY = value;
                         MPVCore::instance().restart();
                     });
}

SettingActivity::~SettingActivity() {
    brls::Logger::debug("SettingActivity: delete");
    if (onCloseCallback) onCloseCallback();
}
