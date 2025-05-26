

#pragma once

#include <borealis/core/box.hpp>
#include <borealis/core/bind.hpp>

class ButtonClose;
class TsVitchSelectorCell;
enum class SettingItem;
namespace brls {
class ScrollingFrame;
class Header;
class BooleanCell;
class DetailCell;
class SliderCell;
class RadioCell;
class InputNumericCell;
};  // namespace brls

class PlayerSetting : public brls::Box {
public:
    PlayerSetting();

    bool isTranslucent() override;

    View* getDefaultFocus() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    ~PlayerSetting() override;

    static View* create();

    void setupCommonSetting();

private:
    BRLS_BIND(ButtonClose, closebtn, "button/close");
    BRLS_BIND(brls::ScrollingFrame, settings, "player/settings");
    BRLS_BIND(brls::Box, cancel, "player/cancel");

    BRLS_BIND(brls::BooleanCell, btnFullscreen, "setting/fullscreen");
    BRLS_BIND(TsVitchSelectorCell, btnOnTopMode, "setting/onTopMode");

    BRLS_BIND(brls::BooleanCell, btnMirror, "setting/video/mirror");

    BRLS_BIND(brls::DetailCell, btnSleep, "setting/sleep");

    BRLS_BIND(brls::RadioCell, btnEqualizerReset, "setting/equalizer/reset");
    BRLS_BIND(brls::SliderCell, btnEqualizerBrightness, "setting/equalizer/brightness");
    BRLS_BIND(brls::SliderCell, btnEqualizerContrast, "setting/equalizer/contrast");
    BRLS_BIND(brls::SliderCell, btnEqualizerSaturation, "setting/equalizer/saturation");
    BRLS_BIND(brls::SliderCell, btnEqualizerGamma, "setting/equalizer/gamma");
    BRLS_BIND(brls::SliderCell, btnEqualizerHue, "setting/equalizer/hue");

    BRLS_BIND(brls::Header, bangumiHeader, "setting/video/custom/header");
    BRLS_BIND(brls::Box, bangumiBox, "setting/video/custom/box");
    BRLS_BIND(TsVitchSelectorCell, btnCustomAspect, "setting/video/custom/aspect");
    BRLS_BIND(brls::BooleanCell, btnClip, "setting/video/custom/clip");
    BRLS_BIND(brls::InputNumericCell, btnClipStart, "setting/video/custom/clip/start");
    BRLS_BIND(brls::InputNumericCell, btnClipEnd, "setting/video/custom/clip/end");

    void updateCountdown(size_t now);

    void setupEqualizerSetting(brls::SliderCell* cell, const std::string& title, SettingItem item, int initValue);

    void registerHideBackground(brls::View* view);
};