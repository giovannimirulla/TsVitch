#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include <borealis/core/application.hpp>

#include "utils/event_helper.hpp"

namespace brls {
class Label;
class ProgressSpinner;
}  // namespace brls
class MPVCore;
class VideoProgressSlider;
class SVGImage;
class VideoProfile;

class EDLUrl {
public:
    std::string url;
    float length = -1;

    EDLUrl(std::string url, float length = -1) : url(url), length(length) {}
};

enum class OSDState {
    HIDDEN    = 0,
    SHOWN     = 1,
    ALWAYS_ON = 2,
};

class VideoHighlightData {
public:
    int sec = 0;
    std::vector<float> data;
};

#define VIDEO_CANCEL_SEEKING 0
#define VIDEO_SEEK_IMMEDIATELY 0

class VideoView : public brls::Box {
public:
    VideoView();

    ~VideoView() override;

    void setUrl(const std::string& url, int start = 0, int end = -1, const std::string& audio = "");

    void setUrl(const std::string& url, int start, int end, const std::vector<std::string>& audios);

    void setUrl(const std::vector<EDLUrl>& edl_urls, int start = 0, int end = -1);

    static std::string genExtraUrlParam(int start, int end, const std::string& audio);

    static std::string genExtraUrlParam(int start, int end, const std::vector<std::string>& audios = {});

    void resume();

    void pause();

    void stop();

    void togglePlay();

    void setSpeed(float speed);

    void setLastPlayedPosition(int64_t p);
    [[nodiscard]] int64_t getLastPlayedPosition() const;

    void showOSD(bool temp = true);

    void hideOSD();

    bool isOSDShown() const;

    bool isOSDLock() const;

    void onOSDStateChanged(bool state);

    void toggleOSDLock();

    void toggleOSD();

    void showLoading();

    void hideLoading();

    void showCenterHint();

    void setCenterHintText(const std::string& text);

    void setCenterHintIcon(const std::string& svg);

    void hideCenterHint();

    void hideOSDLockButton();

    void disableCloseOnEndOfFile();

    void hideHistorySetting();

    void hideVideoRelatedSetting();

    void hideSubtitleSetting();

    void hideBottomLineSetting();

    void hideHighlightLineSetting();

    void hideVideoProgressSlider();

    void hideStatusLabel();

    void setLiveMode();

    void setTvControlMode(bool state);

    bool getTvControlMode() const;

    void setStatusLabelLeft(const std::string& value);

    void setStatusLabelRight(const std::string& value);

    void setCustomToggleAction(std::function<void()> action);

    void setTitle(const std::string& title);

    std::string getTitle();

    void setDuration(const std::string& value);

    void setPlaybackTime(const std::string& value);

    void setFullscreenIcon(bool fs);

    void setFavoriteIcon(bool fs);

    void setOSDSliderFocusable(bool state);

    brls::View* getFullscreenIcon();
    
    brls::View* getFavoriteIcon();

    void refreshToggleIcon();

    void setProgress(float value);

    float getProgress();

    void setHighlightProgress(const VideoHighlightData& data);

    void showHint(const std::string& value);

    void clearHint();

    static View* create();

    void invalidate() override;

    void onLayout() override;

    bool isFullscreen();

    void setFullScreen(bool fs);

    void toggleFavorite();

    void setFavoriteCallback(std::function<void(bool)> callback);

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    View* getDefaultFocus() override;

    void onChildFocusGained(View* directChild, View* focusedView) override;

    View* getNextFocus(brls::FocusDirection direction, View* currentView) override;

    void registerMpvEvent();

    void unRegisterMpvEvent();

    void buttonProcessing();


    void setOnEndCallback(std::function<void()> callback);

    inline static const std::string QUALITY_CHANGE = "QUALITY_CHANGE";
    inline static const std::string SET_ONLINE_NUM = "SET_ONLINE_NUM";
    inline static const std::string SET_TITLE      = "SET_TITLE";
    inline static const std::string SET_QUALITY    = "SET_QUALITY";
    inline static const std::string HINT           = "HINT";
    inline static const std::string LAST_TIME      = "LAST_TIME";
    inline static const std::string REPLAY         = "REPLAY";
    inline static const std::string CLIP_INFO      = "CLIP_INFO";
    inline static const std::string HIGHLIGHT_INFO = "HIGHLIGHT_INFO";
    inline static const std::string REAL_DURATION  = "REAL_DURATION";

    inline static const int64_t POSITION_UNDEFINED = -1;
    inline static const int64_t POSITION_DISCARD   = -2;

    inline static bool EXIT_FULLSCREEN_ON_END = true;

    inline static bool BOTTOM_BAR = true;

    inline static bool HIGHLIGHT_PROGRESS_BAR = false;

private:
    bool allowFullscreen  = true;
    bool registerMPVEvent = false;

    bool closeOnEndOfFile = true;

    bool showHistorySetting = true;

    bool showVideoRelatedSetting = true;

    bool showSubtitleSetting = true;

    bool showBottomLineSetting = true;

    bool showHighlightLineSetting = true;

    bool isLiveMode = false;

    bool isTvControlMode = false;

    bool isFavorite = false;

    std::function<void(bool)> favoriteCallback = nullptr;

    bool showReplay = false;
    std::string bangumiTitle;
    MPVEvent::Subscription eventSubscribeID;
    CustomEvent::Subscription customEventSubscribeID;
    std::function<void()> customToggleAction = nullptr;
    brls::InputManager* input;
    NVGcolor bottomBarColor = brls::Application::getTheme().getColor("color/tsvitch");

    BRLS_BIND(brls::Label, videoTitleLabel, "video/osd/title");
    BRLS_BIND(brls::Box, osdTopBox, "video/osd/top/box");
    BRLS_BIND(brls::Box, osdBottomBox, "video/osd/bottom/box");

    BRLS_BIND(brls::Box, osdCenterBox, "video/osd/center/box");
    BRLS_BIND(brls::ProgressSpinner, osdSpinner, "video/osd/loading");
    BRLS_BIND(brls::Label, centerLabel, "video/osd/center/label");

    BRLS_BIND(brls::Box, osdCenterBox2, "video/osd/center/box2");
    BRLS_BIND(brls::Label, centerLabel2, "video/osd/center/label2");
    BRLS_BIND(SVGImage, centerIcon2, "video/osd/center/icon2");

    BRLS_BIND(VideoProgressSlider, osdSlider, "video/osd/bottom/progress");
    BRLS_BIND(brls::Label, leftStatusLabel, "video/left/status");
    BRLS_BIND(brls::Label, centerStatusLabel, "video/center/status");
    BRLS_BIND(brls::Label, rightStatusLabel, "video/right/status");

    BRLS_BIND(brls::Box, btnToggle, "video/osd/toggle");

    BRLS_BIND(brls::Box, iconBox, "video/osd/icon/box");
    BRLS_BIND(SVGImage, btnToggleIcon, "video/osd/toggle/icon");
    BRLS_BIND(SVGImage, btnFullscreenIcon, "video/osd/fullscreen/icon");
    BRLS_BIND(SVGImage, btnFavoriteIcon, "video/osd/favorite/icon");

    BRLS_BIND(SVGImage, btnVolumeIcon, "video/osd/danmaku/volume/icon");

    BRLS_BIND(SVGImage, btnSettingIcon, "video/osd/setting/icon");

    BRLS_BIND(brls::Label, hintLabel, "video/osd/hint/label");
    BRLS_BIND(brls::Box, hintBox, "video/osd/hint/box");
    BRLS_BIND(VideoProfile, videoProfile, "video/profile");
    BRLS_BIND(brls::Box, osdLockBox, "video/osd/lock/box");
    BRLS_BIND(SVGImage, osdLockIcon, "video/osd/lock/icon");

    time_t osdLastShowTime     = 0;
    const time_t OSD_SHOW_TIME = 5;
    OSDState osd_state         = OSDState::HIDDEN;
    bool is_osd_shown          = false;
    bool is_osd_lock           = false;
    bool hide_lock_button      = false;

    int real_duration          = 0;
    time_t hintLastShowTime    = 0;
    int64_t lastPlayedPosition = POSITION_UNDEFINED;
    VideoHighlightData highlightData;

    MPVCore* mpvCore;
    brls::Rect oldRect = brls::Rect(-1, -1, -1, -1);
    
    void requestSeeking(int seek, int delay = 400);

    bool is_seeking     = false;
    int seeking_range   = 0;
    size_t seeking_iter = 0;

    void requestVolume(int volume, int delay = 0);
    int volume_init    = 0;
    size_t volume_iter = 0;

    void requestBrightness(float brightness);
    float brightness_init = 0.0f;

    void drawHighlightProgress(NVGcontext* vg, float x, float y, float width, float alpha);

    void _setTvControlMode(bool state);

    float getRealDuration();

private:
    std::function<void()> onEndCallback = nullptr;
};
