#pragma once

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>

namespace brls {
class RadioCell;
class BooleanCell;
class InputCell;
class Label;
}  // namespace brls
class TextBox;
class TsVitchSelectorCell;

class SettingsActivity : public brls::Activity {
public:
    CONTENT_FROM_XML_RES("activity/settings_activity.xml");

    SettingsActivity(std::function<void()> onClose = nullptr);

    void onContentAvailable() override;

    ~SettingsActivity() override;

private:
    std::function<void()> onCloseCallback;
    
    BRLS_BIND(brls::RadioCell, btnTutorialOpenApp, "tools/tutorial_open");
    BRLS_BIND(brls::RadioCell, btnTutorialError, "tools/tutorial_error");
    BRLS_BIND(brls::RadioCell, btnTutorialFont, "tools/tutorial_font");
    BRLS_BIND(brls::RadioCell, btnNetworkChecker, "tools/network_checker");
    BRLS_BIND(brls::RadioCell, btnProxyTest, "tools/proxy_test");
    BRLS_BIND(brls::RadioCell, btnReleaseChecker, "tools/release_checker");
    BRLS_BIND(brls::RadioCell, btnQuit, "tools/quit");
    BRLS_BIND(brls::RadioCell, btnOpenConfig, "tools/config_dir");
    BRLS_BIND(brls::RadioCell, btnVibrationTest, "tools/vibration_test");
    BRLS_BIND(brls::BooleanCell, btnTls, "setting/network/tls");
    BRLS_BIND(brls::BooleanCell, btnProxy, "setting/network/proxy");
    BRLS_BIND(brls::InputCell, btnM3U8Input, "setting/tools/m3u8/input");
    BRLS_BIND(brls::InputCell, btnProxyInput, "setting/tools/proxy/input");
    BRLS_BIND(TsVitchSelectorCell, selectorM3U8Timeout, "setting/tools/m3u8/timeout");
    
    // IPTV Xtream Codes bindings
    BRLS_BIND(brls::BooleanCell, btnXtreamEnabled, "setting/iptv/xtream_enabled");
    BRLS_BIND(brls::InputCell, btnXtreamServer, "setting/iptv/xtream_server");
    BRLS_BIND(brls::InputCell, btnXtreamUsername, "setting/iptv/xtream_username");
    BRLS_BIND(brls::InputCell, btnXtreamPassword, "setting/iptv/xtream_password");
    
    BRLS_BIND(TsVitchSelectorCell, selectorLang, "setting/language");
    BRLS_BIND(TsVitchSelectorCell, selectorTheme, "setting/ui/theme");
    BRLS_BIND(TsVitchSelectorCell, selectorCustomTheme, "setting/custom/theme");
    BRLS_BIND(TsVitchSelectorCell, selectorUIScale, "setting/ui/scale");
    BRLS_BIND(TsVitchSelectorCell, selectorTexture, "setting/image/texture");
    BRLS_BIND(TsVitchSelectorCell, selectorThreads, "setting/image/threads");
    BRLS_BIND(TsVitchSelectorCell, selectorKeymap, "setting/keymap");
    BRLS_BIND(brls::BooleanCell, btnKeymapSwap, "setting/keymap_swap");
    BRLS_BIND(brls::BooleanCell, btnOpencc, "setting/opencc");
    BRLS_BIND(brls::BooleanCell, btnQuality, "setting/video/quality");
    BRLS_BIND(brls::BooleanCell, btnHWDEC, "setting/video/hwdec");
    // BRLS_BIND(brls::BooleanCell, btnAutoPlay, "setting/video/auto_play");
    BRLS_BIND(TsVitchSelectorCell, selectorInmemory, "setting/video/inmemory");
    BRLS_BIND(TsVitchSelectorCell, selectorFormat, "setting/video/format");
    BRLS_BIND(TsVitchSelectorCell, selectorCodec, "setting/video/codec");
    BRLS_BIND(TsVitchSelectorCell, selectorQuality, "setting/audio/quality");
    BRLS_BIND(TsVitchSelectorCell, selectorFPS, "setting/fps");
    BRLS_BIND(brls::Box, boxOpensource, "setting/opensource");
    BRLS_BIND(brls::BooleanCell, cellShowBar, "cell/showBottomBar");
    BRLS_BIND(brls::BooleanCell, cellShowFPS, "cell/showFPS");
    BRLS_BIND(brls::BooleanCell, cellTvSearch, "cell/tvSearch");
    BRLS_BIND(brls::BooleanCell, cellTvOSD, "cell/tvOSD");
    BRLS_BIND(brls::BooleanCell, cellFullscreen, "cell/fullscreen");
    BRLS_BIND(TsVitchSelectorCell, cellOnTopMode, "cell/onTopMode");
    BRLS_BIND(brls::BooleanCell, cellVibration, "cell/gamepadVibration");
    BRLS_BIND(brls::Label, labelAboutVersion, "setting/about/version");
};