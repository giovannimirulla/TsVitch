#include <limits>

#include <borealis/views/label.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/slider.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/dialog.hpp>
#include <pystring.h>
#include <cmath> 

#include "utils/number_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/switch_helper.hpp"

#include "utils/gesture_helper.hpp"
#include "utils/activity_helper.hpp"

#include "fragment/player_setting.hpp"

#include "view/video_view.hpp"

#include "view/video_progress_slider.hpp"
#include "view/svg_image.hpp"
#include "view/grid_dropdown.hpp"
#include "view/video_profile.hpp"

#include "view/mpv_core.hpp"
#ifdef __SWITCH__
#include <switch.h>
#endif
using namespace brls::literals;

enum ClickState { IDLE = 0, PRESS = 1, FAST_RELEASE = 3, FAST_PRESS = 4, CLICK_DOUBLE = 5 };

#define CHECK_OSD(shake)                                                              \
    if (is_osd_lock) {                                                                \
        if (isOSDShown()) {                                                           \
            brls::Application::giveFocus(this->osdLockBox);                           \
            if (shake) this->osdLockBox->shakeHighlight(brls::FocusDirection::RIGHT); \
        } else {                                                                      \
            this->showOSD(true);                                                      \
        }                                                                             \
        return true;                                                                  \
    }

VideoView::VideoView() {
    mpvCore = &MPVCore::instance();
    this->inflateFromXMLRes("xml/views/video_view.xml");
    androidInitialized = true;
    this->setHideHighlightBackground(true);
    this->setHideClickAnimation(true);

    setTvControlMode(ProgramConfig::instance().getBoolOption(SettingItem::PLAYER_OSD_TV_MODE));

    input = brls::Application::getPlatform()->getInputManager();

    this->registerBoolXMLAttribute("allowFullscreen", [this](bool value) {
        this->allowFullscreen = value;
        if (!value && this->btnFullscreenIcon) {
            this->btnFullscreenIcon->getParent()->setVisibility(brls::Visibility::GONE);
            this->registerAction(
                "cancel", brls::ControllerButton::BUTTON_B,
                [this](brls::View* view) -> bool {
                    this->dismiss();
                    return true;
                },
                true);
        }
    });
    brls::Logger::info("VideoView: registerBoolXMLAttribute allowFullscreen done");
    this->registerBoolXMLAttribute("disabledSliderGesture", [this](bool value) {
#ifdef __ANDROID__
        this->disabledSliderGesture = true; // Always disabled on Android (slider gesture causes crashes)
#else
        this->disabledSliderGesture = value;
        if (!this->disabledSliderGesture) {
            osdSlider->getProgressSetEvent()->subscribe([this](float progress) {
                brls::Logger::verbose("Set progress: {}", progress);
                this->showOSD(true);
                if (real_duration > 0) {
                    mpvCore->seek((float)real_duration * progress);
                } else {
                    mpvCore->seekPercent(progress);
                }
            });

            osdSlider->getProgressEvent()->subscribe([this](float progress) {
                this->showOSD(false);
                leftStatusLabel->setText(tsvitch::sec2Time(getRealDuration() * progress));
            });
        }
#endif
    });
#ifdef __ANDROID__
    this->disabledSliderGesture = true;
#endif
    brls::Logger::info("VideoView: disabledSliderGesture registered");

#ifndef __ANDROID__
    this->registerAction(
        "toggleOSD", brls::ControllerButton::BUTTON_Y,
        [this](brls::View* view) -> bool {
            if (is_seeking) return true;
            this->toggleOSD();
            return true;
        },
        true);

    this->registerAction(
        "volumeUp", brls::ControllerButton::BUTTON_NAV_UP,
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            brls::ControllerState state{};
            input->updateUnifiedControllerState(&state);
            if (state.buttons[brls::BUTTON_RT]) {
                this->requestVolume((int)MPVCore::instance().volume + 5, 400);
                return true;
            }
            return false;
        },
        true, true);

    this->registerAction(
        "volumeDown", brls::ControllerButton::BUTTON_NAV_DOWN,
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            brls::ControllerState state{};
            input->updateUnifiedControllerState(&state);
            if (state.buttons[brls::BUTTON_RT]) {
                this->requestVolume((int)MPVCore::instance().volume - 5, 400);
                return true;
            }
            return false;
        },
        true, true);
#endif // !__ANDROID__

    this->registerMpvEvent();

    brls::Logger::info("VideoView: about to addGestureRecognizer OsdGestureRecognizer");
#ifndef __ANDROID__
    this->addGestureRecognizer(new OsdGestureRecognizer([this](OsdGestureStatus status) {
        switch (status.osdGestureType) {
            case OsdGestureType::TAP:
                this->toggleOSD();
                break;
            case OsdGestureType::DOUBLE_TAP_END:
                if (is_osd_lock) {
                    this->toggleOSD();
                    break;
                }
                this->togglePlay();
                break;
            case OsdGestureType::HORIZONTAL_PAN_START:
                if (is_osd_lock || this->disabledSliderGesture) break;
                this->showCenterHint();
                this->setCenterHintIcon("svg/arrow-left-right.svg");
                break;
            case OsdGestureType::HORIZONTAL_PAN_UPDATE:
                if (is_osd_lock || this->disabledSliderGesture) break;
                this->requestSeeking(fmin(120.0f, getRealDuration()) * status.deltaX);
                break;
            case OsdGestureType::HORIZONTAL_PAN_CANCEL:
                if (is_osd_lock || this->disabledSliderGesture) break;

                this->requestSeeking(VIDEO_CANCEL_SEEKING, VIDEO_SEEK_IMMEDIATELY);
                break;
            case OsdGestureType::HORIZONTAL_PAN_END:
                if (is_osd_lock) {
                    this->toggleOSD();
                    break;
                }
                if (this->disabledSliderGesture) break;
                this->requestSeeking(fmin(120.0f, getRealDuration()) * status.deltaX, VIDEO_SEEK_IMMEDIATELY);
                break;
            case OsdGestureType::LEFT_VERTICAL_PAN_START:
                if (is_osd_lock) break;
                if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                    this->brightness_init = brls::Application::getPlatform()->getBacklightBrightness();
                    this->showCenterHint();
                    this->setCenterHintIcon("svg/sun-fill.svg");
                    break;
                }
            case OsdGestureType::RIGHT_VERTICAL_PAN_START:
                if (is_osd_lock) break;
                this->volume_init = (int)MPVCore::instance().volume;
                this->showCenterHint();
                this->setCenterHintIcon("svg/bpx-svg-sprite-volume.svg");
                break;
            case OsdGestureType::LEFT_VERTICAL_PAN_UPDATE:
                if (is_osd_lock) break;
                if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                    this->requestBrightness(this->brightness_init + status.deltaY);
                    break;
                }
            case OsdGestureType::RIGHT_VERTICAL_PAN_UPDATE:
                if (is_osd_lock) break;
                this->requestVolume(this->volume_init + status.deltaY * 100);
                break;
            case OsdGestureType::LEFT_VERTICAL_PAN_CANCEL:
            case OsdGestureType::LEFT_VERTICAL_PAN_END:
                if (is_osd_lock) {
                    this->toggleOSD();
                    break;
                }
                if (brls::Application::getPlatform()->canSetBacklightBrightness()) {
                    this->hideCenterHint();
                    break;
                }
            case OsdGestureType::RIGHT_VERTICAL_PAN_CANCEL:
            case OsdGestureType::RIGHT_VERTICAL_PAN_END:
                if (is_osd_lock) {
                    this->toggleOSD();
                    break;
                }
                this->hideCenterHint();
                ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_VOLUME, MPVCore::VIDEO_VOLUME);
                break;
            default:
                break;
        }
    }));
#endif // !__ANDROID__

    brls::Logger::info("VideoView: about to access btnToggle");
    // Register play/pause toggle — works on all platforms
    if (this->btnToggle) {
        this->btnToggle->addGestureRecognizer(
            new brls::TapGestureRecognizer(this->btnToggle, [this]() { this->togglePlay(); }));
        this->btnToggle->registerClickAction([this](...) {
            this->togglePlay();
            return true;
        });
    }
    brls::Logger::info("VideoView: btnToggle done");

    brls::Logger::info("VideoView: about to register profile action");
#ifndef __ANDROID__
    this->registerAction(
        "profile", brls::ControllerButton::BUTTON_BACK,
        [this](brls::View* view) -> bool {
            CHECK_OSD(true);
            if (!videoProfile) return true;
            if (videoProfile->getVisibility() == brls::Visibility::VISIBLE) {
                videoProfile->setVisibility(brls::Visibility::INVISIBLE);
                return true;
            }
            videoProfile->setVisibility(brls::Visibility::VISIBLE);
            videoProfile->update();
            return true;
        },
        true);
#endif // !__ANDROID__
    brls::Logger::info("VideoView: profile action registered");

    brls::Logger::info("VideoView: about to check btnFullscreenIcon");
    // Back button — works on all platforms
    {
        brls::Box* backBox = static_cast<brls::Box*>(this->getView("video/osd/back"));
        if (backBox) {
            backBox->registerClickAction([this](...) {
#ifdef __ANDROID__
                // On Android: pop the activity directly
                brls::Application::popActivity();
#else
                if (this->isFullscreen()) {
                    this->setFullScreen(false);
                } else {
                    this->dismiss();
                }
#endif
                return true;
            });
            backBox->addGestureRecognizer(new brls::TapGestureRecognizer(backBox));
        }
    }

#ifndef __ANDROID__
    if (this->btnFullscreenIcon) {
        this->btnFullscreenIcon->getParent()->registerClickAction([this](...) {
            if (this->isFullscreen()) {
                this->setFullScreen(false);
            } else {
                this->setFullScreen(true);
            }
            return true;
        });
        this->btnFullscreenIcon->getParent()->addGestureRecognizer(
            new brls::TapGestureRecognizer(this->btnFullscreenIcon->getParent()));
    }
    if (this->btnFavoriteIcon) {
        this->btnFavoriteIcon->getParent()->registerClickAction([this](...) {
            this->toggleFavorite();
            return true;
        });
        this->btnFavoriteIcon->getParent()->addGestureRecognizer(
            new brls::TapGestureRecognizer(this->btnFavoriteIcon->getParent()));
    }

    if (this->btnSettingIcon) {
        this->btnSettingIcon->getParent()->registerClickAction([](...) {
            auto setting = new PlayerSetting();

            brls::Application::pushActivity(new brls::Activity(setting));
            GA("open_player_setting")
            return true;
        });
        this->btnSettingIcon->getParent()->addGestureRecognizer(
            new brls::TapGestureRecognizer(this->btnSettingIcon->getParent()));
    }

    if (this->btnVolumeIcon) {
        this->btnVolumeIcon->getParent()->registerClickAction([this](brls::View* view) {
        this->showOSD(false);
        auto theme     = brls::Application::getTheme();
        auto container = new brls::Box();
        container->setHideClickAnimation(true);
        container->addGestureRecognizer(new brls::TapGestureRecognizer(container, [this, container]() {
            this->showOSD(true);
            container->dismiss();

            ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_VOLUME, MPVCore::VIDEO_VOLUME);
        }));

        auto sliderBox = new brls::Box();
        sliderBox->setAlignItems(brls::AlignItems::CENTER);
        sliderBox->setHeight(60);
        sliderBox->setCornerRadius(4);
        sliderBox->setBackgroundColor(theme.getColor("color/grey_1"));
        float sliderX = view->getX() - 120;
        if (sliderX < 0) sliderX = 20;
        if (sliderX > brls::Application::ORIGINAL_WINDOW_WIDTH - 332)
            sliderX = brls::Application::ORIGINAL_WINDOW_WIDTH - 332;
        sliderBox->setTranslationX(sliderX);
        sliderBox->setTranslationY(view->getY() - 70);

        auto slider = new brls::Slider();
        slider->setMargins(8, 16, 8, 16);
        slider->setWidth(300);
        slider->setHeight(40);
        slider->setProgress(MPVCore::instance().getVolume() * 1.0 / 100);
        slider->getProgressEvent()->subscribe([](float progress) { MPVCore::instance().setVolume(progress * 100); });
        sliderBox->addView(slider);
        container->addView(sliderBox);
        auto frame = new brls::AppletFrame(container);
        frame->setInFadeAnimation(true);
        frame->setHeaderVisibility(brls::Visibility::GONE);
        frame->setFooterVisibility(brls::Visibility::GONE);
        frame->setBackgroundColor(theme.getColor("brls/backdrop"));

        brls::Application::pushActivity(new brls::Activity(frame));
        return true;
    });
    this->btnVolumeIcon->getParent()->addGestureRecognizer(
            new brls::TapGestureRecognizer(this->btnVolumeIcon->getParent()));
        if (mpvCore->volume <= 0) {
            this->btnVolumeIcon->setImageFromSVGRes("svg/bpx-svg-sprite-volume-off.svg");
        }
    }  // end if (btnVolumeIcon)
#endif // !__ANDROID__

#ifdef __ANDROID__
    // Tap on video area → toggle OSD (Android touch via mouse events)
    // updateTouchStates is disabled on Android to avoid double-triggering,
    // so we use a TapGestureRecognizer that works via the mouse path.
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this, [this]() {
        this->toggleOSD();
    }));

    // Fullscreen button → dismiss on Android (no real fullscreen toggle needed)
    {
        brls::Box* fsBox = static_cast<brls::Box*>(this->getView("video/osd/fullscreen"));
        if (fsBox) {
            fsBox->registerClickAction([this](...) {
                this->dismiss();
                return true;
            });
            fsBox->addGestureRecognizer(new brls::TapGestureRecognizer(fsBox));
        }
    }

    // Volume button → mute/unmute on Android
    {
        brls::Box* volBox = static_cast<brls::Box*>(this->getView("video/osd/danmaku/box/volume"));
        if (volBox) {
            volBox->registerClickAction([this](...) {
                int64_t vol = MPVCore::instance().getVolume();
                MPVCore::instance().setVolume(vol > 0 ? 0 : 100);
                return true;
            });
            volBox->addGestureRecognizer(new brls::TapGestureRecognizer(volBox));
        }
    }

    // Favorite button → toggle favorite on Android
    {
        brls::Box* favBox = static_cast<brls::Box*>(this->getView("video/osd/favorite"));
        if (favBox) {
            favBox->registerClickAction([this](...) {
                this->toggleFavorite();
                return true;
            });
            favBox->addGestureRecognizer(new brls::TapGestureRecognizer(favBox));
        }
    }

    // Settings button → open player settings on Android
    {
        brls::View* settingView = this->getView("video/osd/setting");
        brls::Logger::debug("VideoView: Android settings button view: {}", settingView ? "found" : "null");
        if (settingView) {
            settingView->registerClickAction([this](...) {
                auto setting = new PlayerSetting();
                brls::Application::pushActivity(new brls::Activity(setting));
                GA("open_player_setting")
                return true;
            });
            settingView->addGestureRecognizer(new brls::TapGestureRecognizer(settingView));
        } else {
            brls::Logger::warning("VideoView: video/osd/setting not found on Android");
        }
    }
#endif // __ANDROID__

    brls::Logger::info("VideoView: icon buttons done");

    brls::Logger::info("VideoView: about to register osdLockBox actions");
    this->osdLockBox->registerClickAction([this](...) {
        this->toggleOSDLock();
        return true;
    });
    this->osdLockBox->addGestureRecognizer(new brls::TapGestureRecognizer(this->osdLockBox));
    brls::Logger::info("VideoView: osdLockBox actions registered");

#ifndef __ANDROID__
    this->registerAction(
        "cancel", brls::ControllerButton::BUTTON_B,
        [this](brls::View* view) -> bool {
            if (is_osd_lock) {
                this->toggleOSD();
                return true;
            }
            if (this->isFullscreen()) {
                if (isTvControlMode && isOSDShown()) {
                    this->toggleOSD();
                    return true;
                }
                this->setFullScreen(false);
            } else {
                this->dismiss();
            }
            return true;
        },
        true);
#endif // !__ANDROID__

    brls::Logger::info("VideoView: about to subscribe customEvent");
    customEventSubscribeID = APP_E->subscribe([this](const std::string& event, void* data) {
        if (event == VideoView::SET_TITLE) {
            this->setTitle((const char*)data);
        } else if (event == VideoView::REAL_DURATION) {
            this->real_duration = *(int*)data;
            this->setDuration(tsvitch::sec2Time(real_duration));
            this->setProgress((float)mpvCore->playback_time / (float)real_duration);
        } else if (event == VideoView::LAST_TIME) {
            if (*(int64_t*)data == VideoView::POSITION_DISCARD)
                this->setLastPlayedPosition(VideoView::POSITION_DISCARD);
            else if (this->getLastPlayedPosition() != VideoView::POSITION_DISCARD) {
                int64_t p = *(int64_t*)data / 1000;
                this->setLastPlayedPosition(p);
                mpvCore->seek(p);
            }
        } else if (event == VideoView::HINT) {
            this->showHint((const char*)data);
        } else if (event == VideoView::CLIP_INFO) {
            if (osdSlider) osdSlider->addClipPoint(*(float*)data);
        } else if (event == VideoView::HIGHLIGHT_INFO) {
            this->setHighlightProgress(*(VideoHighlightData*)data);
        }
    });
// #ifdef __ANDROID__  // disabled
//     } catch (const std::exception& e) {
//         brls::Logger::error("VideoView: constructor exception after inflate: {}", e.what());
//     } catch (...) {
//         brls::Logger::error("VideoView: constructor unknown exception after inflate");
//     }
// #endif
    brls::Logger::info("VideoView: constructor COMPLETE");
}

void VideoView::requestVolume(int volume, int delay) {
    if (volume < 0) volume = 0;
    if (volume > 100) volume = 100;
    MPVCore::instance().setVolume(volume);
    setCenterHintText(fmt::format("{} %", volume));
    if (delay == 0) return;
    if (volume_iter == 0) {
        this->showCenterHint();
        this->setCenterHintIcon("svg/bpx-svg-sprite-volume.svg");
    } else {
        brls::cancelDelay(volume_iter);
    }
    ASYNC_RETAIN
    volume_iter = brls::delay(delay, [ASYNC_TOKEN]() {
        ASYNC_RELEASE
        this->hideCenterHint();
        ProgramConfig::instance().setSettingItem(SettingItem::PLAYER_VOLUME, MPVCore::VIDEO_VOLUME);
        this->volume_iter = 0;
    });
}

void VideoView::requestBrightness(float brightness) {
    if (brightness < 0) brightness = 0.0f;
    if (brightness > 1) brightness = 1.0f;
    brls::Application::getPlatform()->setBacklightBrightness(brightness);
    setCenterHintText(fmt::format("{} %", (int)(brightness * 100)));
}

void VideoView::requestSeeking(int seek, int delay) {
    if (getRealDuration() <= 0) {
        seeking_range = 0;
        is_seeking    = false;
        return;
    }
    double progress = (this->mpvCore->playback_time + seek) / getRealDuration();

    if (progress < 0) {
        progress = 0;
        seek     = (int64_t)this->mpvCore->playback_time * -1;
    } else if (progress > 1) {
        progress = 1;
        seek     = getRealDuration();
    }

    showOSD(false);
    if (osdCenterBox2->getVisibility() != brls::Visibility::VISIBLE) {
        showCenterHint();
        setCenterHintIcon("svg/arrow-left-right.svg");
    }
    setCenterHintText(fmt::format("{:+d} s", seek));
#ifndef __ANDROID__
    if (osdSlider) osdSlider->setProgress((float)progress);
#endif
    leftStatusLabel->setText(tsvitch::sec2Time(getRealDuration() * progress));

    brls::cancelDelay(seeking_iter);
    if (delay <= 0) {
        this->hideCenterHint();
        seeking_range = 0;
        is_seeking    = false;
        if (seek == 0) return;
        mpvCore->seekRelative(seek);
    } else {
        is_seeking = true;
        ASYNC_RETAIN
        seeking_iter = brls::delay(delay, [ASYNC_TOKEN, seek]() {
            ASYNC_RELEASE
            this->hideCenterHint();
            seeking_range = 0;
            is_seeking    = false;
            if (seek == 0) return;
            mpvCore->seekRelative(seek);
        });
    }
}

#ifdef __ANDROID__
void VideoView::willAppear(bool resetState) {
    // On Android, ProgressSpinner::willAppear() crashes because it starts
    // an animation that is not safe to run during the initial layout pass.
    // Skip willAppear for the spinner; it will be started explicitly in showLoading().
    brls::ProgressSpinner* spinner = this->osdSpinner;
    for (brls::View* child : this->getChildren()) {
        if (child == spinner) continue;
        if (!child) continue;
        uintptr_t vtablePtr = *reinterpret_cast<uintptr_t*>(child);
        if (vtablePtr < 0x10000) {
            brls::Logger::error("VideoView::willAppear: corrupted vtable for child {}", (void*)child);
            continue;
        }
        child->willAppear(resetState);
    }
}
#endif

VideoView::~VideoView() {    brls::Logger::debug("trying delete VideoView...");
    this->unRegisterMpvEvent();
    APP_E->unsubscribe(customEventSubscribeID);
#ifdef __SWITCH__
    HidsysNotificationLedPattern pattern = SwitchHelper::getClearPattern();
    SwitchHelper::sendLedPattern(pattern);
#endif
    brls::Logger::debug("Delete VideoView done");
}

void VideoView::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                     brls::FrameContext* ctx) {
    if (!mpvCore->isValid()) return;
    if (!androidInitialized) return;  // Android: not fully initialized
    float alpha    = this->getAlpha();
    time_t current = tsvitch::unix_time();
    bool drawOSD   = current < this->osdLastShowTime;

    mpvCore->draw(brls::Rect(x, y, width, height), alpha);

    if (HIGHLIGHT_PROGRESS_BAR && (!drawOSD || is_osd_lock)) {
        drawHighlightProgress(vg, x, y + height, width, alpha);
    }

    if (BOTTOM_BAR && showBottomLineSetting) {
        bottomBarColor.a = alpha;
        float progress   = mpvCore->playback_time / getRealDuration();
        progress         = progress > 1.0f ? 1.0f : progress;
        nvgFillColor(vg, bottomBarColor);
        nvgBeginPath(vg);
        nvgRect(vg, x, y + height - 2, width * progress, 2);
        nvgFill(vg);
    }

    if (drawOSD) {
        if (!is_osd_shown) {
            is_osd_shown = true;
            this->onOSDStateChanged(true);
        }

        if (!is_osd_lock) {
            if (osdSlider) {
                auto sliderRect = osdSlider->getFrame();
                drawHighlightProgress(vg, sliderRect.getMinX() + 30, sliderRect.getMinY() + 25, sliderRect.getWidth() - 60,
                                      alpha);
            }

            osdTopBox->setVisibility(brls::Visibility::VISIBLE);
            osdBottomBox->setVisibility(brls::Visibility::VISIBLE);
            osdBottomBox->frame(ctx);
            osdTopBox->frame(ctx);
        }

        if (!hide_lock_button) {
            osdLockBox->setVisibility(brls::Visibility::VISIBLE);
            osdLockBox->frame(ctx);
        }
    } else {
        if (is_osd_shown) {
            is_osd_shown = false;
            this->onOSDStateChanged(false);
        }
        osdTopBox->setVisibility(brls::Visibility::INVISIBLE);
        osdBottomBox->setVisibility(brls::Visibility::INVISIBLE);
        osdLockBox->setVisibility(brls::Visibility::INVISIBLE);
    }

    if (current > this->hintLastShowTime && this->hintBox->getVisibility() == brls::Visibility::VISIBLE) {
        this->clearHint();
    }

    this->buttonProcessing();

    osdCenterBox->frame(ctx);

    osdCenterBox2->frame(ctx);

    if (videoProfile) videoProfile->frame(ctx);
}

void VideoView::drawHighlightProgress(NVGcontext* vg, float x, float y, float width, float alpha) {
    if (highlightData.data.size() <= 1) return;
    nvgBeginPath(vg);
    nvgFillColor(vg, nvgRGBAf(1.0f, 1.0f, 1.0f, 0.5f * alpha));
    float baseY  = y;
    float dX     = width / ((float)highlightData.data.size() - 1);
    float halfDx = dX / 2;
    float pointX = x, lastX = x;
    float lastY = baseY;
    nvgMoveTo(vg, lastX, lastY);
    lastY -= 12;
    nvgLineTo(vg, lastX, lastY);

    for (size_t i = 1; i < highlightData.data.size(); i++) {
        float item = highlightData.data[i];
        pointX += dX;
        float pointY = baseY - 12 - item * 48;
        float cx     = lastX + halfDx;
#ifdef __PS4__
        if (fabs(lastY - pointY) < 10) {
#else
        if (fabs(lastY - pointY) < 3) {
#endif
            nvgLineTo(vg, pointX, pointY);
        } else {
            nvgBezierTo(vg, cx, lastY, cx, pointY, pointX, pointY);
        }
        lastX = pointX;
        lastY = pointY;
    }
    nvgLineTo(vg, x + width, baseY);
    nvgFill(vg);
}

void VideoView::invalidate() { View::invalidate(); }

void VideoView::onLayout() { brls::View::onLayout(); }

std::string VideoView::genExtraUrlParam(int start, int end, const std::string& audio) {
    std::vector<std::string> audios;
    if (!audio.empty()) {
        audios.emplace_back(audio);
    }
    return genExtraUrlParam(start, end, audios);
}

std::string VideoView::genExtraUrlParam(int start, int end, const std::vector<std::string>& audios) {
    std::string extra = "";

    if (start > 0) {
        extra += fmt::format(",start={}", start);
    }
    if (end > 0) {
        extra += fmt::format(",end={}", end);
    }
    for (auto& audio : audios) extra += fmt::format(",audio-file=\"{}\"", audio);
    return extra;
}

void VideoView::setUrl(const std::string& url, int start, int end, const std::string& audio) {
    std::vector<std::string> audios;
    if (!audio.empty()) {
        audios.emplace_back(audio);
    }
    setUrl(url, start, end, audios);
}

void VideoView::setUrl(const std::string& url, int start, int end, const std::vector<std::string>& audios) {
    mpvCore->setUrl(url, genExtraUrlParam(start, end, audios));
}

void VideoView::setUrl(const std::vector<EDLUrl>& edl_urls, int start, int end) {
    std::string url = "edl://";
    std::vector<std::string> urls;
    bool delay_open = true;
    for (auto& i : edl_urls) {
        if (i.length < 0) {
            delay_open = false;
            break;
        }
    }
    for (auto& i : edl_urls) {
        if (!delay_open) {
            urls.emplace_back(fmt::format("%{}%{}", i.url.size(), i.url));
            continue;
        }
        urls.emplace_back("!delay_open,media_type=video;!delay_open,media_type=audio;" +
                          fmt::format("%{}%{},length={}", i.url.size(), i.url, i.length));
    }
    url += pystring::join(";", urls);
    this->setUrl(url, start, end);
}

void VideoView::resume() { mpvCore->resume(); }

void VideoView::pause() { mpvCore->pause(); }

void VideoView::stop() { mpvCore->stop(); }

void VideoView::togglePlay() {
    if (customToggleAction != nullptr) return customToggleAction();
    if (this->mpvCore->isPaused()) {
        if (showReplay) {
            this->mpvCore->seek(0);
            this->mpvCore->resume();
        } else {
            this->mpvCore->resume();
        }
    } else {
        this->mpvCore->pause();
    }
}

void VideoView::setCustomToggleAction(std::function<void()> action) { this->customToggleAction = action; }

void VideoView::setSpeed(float speed) { mpvCore->setSpeed(speed); }

void VideoView::setLastPlayedPosition(int64_t p) { lastPlayedPosition = p; }

int64_t VideoView::getLastPlayedPosition() const { return lastPlayedPosition; }

void VideoView::showOSD(bool temp) {
    if (temp) {
        this->osdLastShowTime = tsvitch::unix_time() + VideoView::OSD_SHOW_TIME;
        this->osd_state       = OSDState::SHOWN;
    } else {
#ifdef __WINRT__
        this->osdLastShowTime = 0xffffffff;
#else
        this->osdLastShowTime = (std::numeric_limits<std::time_t>::max)();
#endif
        this->osd_state = OSDState::ALWAYS_ON;
    }
}

void VideoView::hideOSD() {
    this->osdLastShowTime = 0;
    this->osd_state       = OSDState::HIDDEN;
}

bool VideoView::isOSDShown() const { return this->is_osd_shown; }

bool VideoView::isOSDLock() const { return this->is_osd_lock; }

void VideoView::onOSDStateChanged(bool state) {
    if (!state && isChildFocused()) {
        brls::Application::giveFocus(this);
    }
}

void VideoView::toggleOSDLock() {
    is_osd_lock = !is_osd_lock;
    if (osdLockIcon) this->osdLockIcon->setImageFromSVGRes(is_osd_lock ? "svg/player-lock.svg" : "svg/player-unlock.svg");
    if (is_osd_lock) {
        osdTopBox->setVisibility(brls::Visibility::INVISIBLE);
        osdBottomBox->setVisibility(brls::Visibility::INVISIBLE);

        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::UP, "video/osd/lock/box");
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, "video/osd/lock/box");
    } else {
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::UP, "video/osd/setting");
        osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, "video/osd/icon/box");
    }
    this->showOSD();
}

void VideoView::toggleOSD() {
    if (isOSDShown()) {
        this->hideOSD();
    } else {
        this->showOSD(true);
    }
}

void VideoView::showLoading() {
    centerLabel->setVisibility(brls::Visibility::INVISIBLE);
    osdCenterBox->setVisibility(brls::Visibility::VISIBLE);

#ifdef __SWITCH__
    HidsysNotificationLedPattern pattern = SwitchHelper::getBreathePattern();
    SwitchHelper::sendLedPattern(pattern);
#endif
}

void VideoView::hideLoading() {
    osdCenterBox->setVisibility(brls::Visibility::GONE);
#ifdef __SWITCH__
    HidsysNotificationLedPattern pattern = SwitchHelper::getClearPattern();
    SwitchHelper::sendLedPattern(pattern);
#endif
}

void VideoView::setCenterHintText(const std::string& text) { centerLabel2->setText(text); }

void VideoView::setCenterHintIcon(const std::string& svg) { if (centerIcon2) centerIcon2->setImageFromSVGRes(svg); }

void VideoView::showCenterHint() { osdCenterBox2->setVisibility(brls::Visibility::VISIBLE); }

void VideoView::hideCenterHint() { osdCenterBox2->setVisibility(brls::Visibility::GONE); }

void VideoView::hideOSDLockButton() {
    osdLockBox->setVisibility(brls::Visibility::INVISIBLE);
    hide_lock_button = true;
}

void VideoView::hideStatusLabel() {
    leftStatusLabel->setVisibility(brls::Visibility::GONE);
    centerStatusLabel->setVisibility(brls::Visibility::GONE);
    rightStatusLabel->setVisibility(brls::Visibility::GONE);
}

void VideoView::setLiveMode() {
    isLiveMode = true;
    leftStatusLabel->setVisibility(brls::Visibility::GONE);
    centerStatusLabel->setVisibility(brls::Visibility::GONE);
    rightStatusLabel->setVisibility(brls::Visibility::GONE);
#ifndef __ANDROID__
    if (osdSlider) osdSlider->setPointerVisible(false);
#endif
    _setTvControlMode(false);
}

void VideoView::setVideoMode() {
    isLiveMode = false;
    leftStatusLabel->setVisibility(brls::Visibility::VISIBLE);
    centerStatusLabel->setVisibility(brls::Visibility::VISIBLE);
    rightStatusLabel->setVisibility(brls::Visibility::VISIBLE);
#ifndef __ANDROID__
    if (osdSlider) osdSlider->setPointerVisible(true);
#endif
    _setTvControlMode(isTvControlMode && !isLiveMode);
}

void VideoView::setAdMode() {
    isLiveMode = false;
    leftStatusLabel->setVisibility(brls::Visibility::VISIBLE);
    centerStatusLabel->setVisibility(brls::Visibility::VISIBLE);
    rightStatusLabel->setVisibility(brls::Visibility::VISIBLE);
#ifndef __ANDROID__
    if (osdSlider) osdSlider->setPointerVisible(false);
#endif
    _setTvControlMode(false);
}

void VideoView::setTvControlMode(bool state) {
    isTvControlMode = state;

    _setTvControlMode(isTvControlMode && !isLiveMode);
}

bool VideoView::getTvControlMode() const { return isTvControlMode; }

void VideoView::_setTvControlMode(bool state) {
    if (!btnToggle || !osdSlider || !iconBox || !btnVolumeIcon || !osdLockBox || !btnFullscreenIcon || !btnFavoriteIcon) return;
    btnToggle->setCustomNavigationRoute(brls::FocusDirection::RIGHT, state ? osdSlider : iconBox);
    btnVolumeIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);

    btnFullscreenIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);
    btnFavoriteIcon->setCustomNavigationRoute(brls::FocusDirection::UP, state ? osdSlider : osdLockBox);

    osdLockBox->setCustomNavigationRoute(brls::FocusDirection::DOWN, state ? osdSlider : iconBox);
#ifndef __ANDROID__
    osdSlider->setFocusable(state);
#endif
}

void VideoView::setStatusLabelLeft(const std::string& value) { leftStatusLabel->setText(value); }

void VideoView::setStatusLabelRight(const std::string& value) { rightStatusLabel->setText(value); }

void VideoView::disableCloseOnEndOfFile() { closeOnEndOfFile = false; }

void VideoView::hideHistorySetting() { showHistorySetting = false; }

void VideoView::hideVideoRelatedSetting() { showVideoRelatedSetting = false; }

void VideoView::hideSubtitleSetting() { showSubtitleSetting = false; }

void VideoView::hideBottomLineSetting() { showBottomLineSetting = false; }

void VideoView::hideHighlightLineSetting() { showHighlightLineSetting = false; }

void VideoView::hideVideoProgressSlider() { if (osdSlider) osdSlider->setVisibility(brls::Visibility::GONE); }

void VideoView::showVideoProgressSlider() { if (osdSlider) osdSlider->setVisibility(brls::Visibility::VISIBLE); }

void VideoView::disableProgressSliderSeek(bool disabled) {
    brls::Logger::debug("VideoView: disableProgressSliderSeek = {}", disabled);
    this->disabledSliderGesture = disabled;
#ifndef __ANDROID__
    if (osdSlider) osdSlider->setDisabledPointerGesture(disabled);
#endif
}

void VideoView::setTitle(const std::string& title) { this->videoTitleLabel->setText(title); }

std::string VideoView::getTitle() { return this->videoTitleLabel->getFullText(); }

void VideoView::setDuration(const std::string& value) { this->rightStatusLabel->setText(value); }

void VideoView::setPlaybackTime(const std::string& value) {
    if (this->is_seeking) return;
    if (this->isLiveMode) return;
    this->leftStatusLabel->setText(value);
}

void VideoView::setFullscreenIcon(bool fs) {
    if (!btnFullscreenIcon) return;
    if (fs) {
        btnFullscreenIcon->setImageFromSVGRes("svg/bpx-svg-sprite-fullscreen-off.svg");
    } else {
        btnFullscreenIcon->setImageFromSVGRes("svg/bpx-svg-sprite-fullscreen.svg");
    }
}

brls::View* VideoView::getFullscreenIcon() { return btnFullscreenIcon; }

void VideoView::setFavoriteIcon(bool fs) {
    if (!btnFavoriteIcon) return;
    brls::Logger::debug("Set favorite icon: {}", fs);
    if (fs) {
        btnFavoriteIcon->setImageFromSVGRes("svg/ico-favorites-activate.svg");
    } else {
        btnFavoriteIcon->setImageFromSVGRes("svg/ico-favorites.svg");
    }
    isFavorite = fs;
}

void VideoView::setFavoriteCallback(std::function<void(bool)> callback) { this->favoriteCallback = callback; }

void VideoView::toggleFavorite() {
    if (favoriteCallback) {
        favoriteCallback(!isFavorite);
    }
    isFavorite = !isFavorite;
    setFavoriteIcon(isFavorite);
}

brls::View* VideoView::getFavoriteIcon() { return btnFavoriteIcon; }

void VideoView::refreshToggleIcon() {
    if (!btnToggleIcon) return;
    if (!mpvCore->isPlaying()) {
        if (showReplay) {
            btnToggleIcon->setImageFromSVGRes("svg/bpx-svg-sprite-re-play.svg");
            return;
        }
        btnToggleIcon->setImageFromSVGRes("svg/bpx-svg-sprite-play.svg");
    } else {
        btnToggleIcon->setImageFromSVGRes("svg/bpx-svg-sprite-pause.svg");
    }
}

void VideoView::setProgress(float value) {
    if (is_seeking) return;
    if (std::isnan(value)) return;
    if (!osdSlider) return;
#ifndef __ANDROID__
    this->osdSlider->setProgress(value);
#endif
}

float VideoView::getProgress() {
#ifndef __ANDROID__
    return osdSlider ? this->osdSlider->getProgress() : 0.0f;
#else
    return 0.0f;
#endif
}

void VideoView::setHighlightProgress(const VideoHighlightData& data) { this->highlightData = data; }

void VideoView::showHint(const std::string& value) {
    brls::Logger::debug("Video hint: {}", value);
    this->hintLabel->setText(value);
    this->hintBox->setVisibility(brls::Visibility::VISIBLE);
    this->hintLastShowTime = tsvitch::unix_time() + VideoView::OSD_SHOW_TIME;
    this->showOSD();
}

void VideoView::clearHint() { this->hintBox->setVisibility(brls::Visibility::GONE); }

brls::View* VideoView::create() { return new VideoView(); }

bool VideoView::isFullscreen() {
    auto rect = this->getFrame();
    return rect.getHeight() == brls::Application::contentHeight && rect.getWidth() == brls::Application::contentWidth;
}

void VideoView::setFullScreen(bool fs) {
    if (!allowFullscreen) {
        brls::Logger::error("Not being allowed to set fullscreen");
        return;
    }

    if (fs == isFullscreen()) {
        brls::Logger::error("Already set fullscreen state to: {}", fs);
        return;
    }

    brls::Logger::info("VideoView set fullscreen state: {}", fs);
    if (fs) {
        this->unRegisterMpvEvent();
        auto container = new brls::Box();
        auto video     = new VideoView();
        float width    = brls::Application::contentWidth;
        float height   = brls::Application::contentHeight;

        container->setDimensions(width, height);
        video->setDimensions(width, height);
        video->setWidthPercentage(100);
        video->setHeightPercentage(100);
        video->setId("video");
        video->setTitle(this->getTitle());
        video->setDuration(this->rightStatusLabel->getFullText());
        video->setPlaybackTime(this->leftStatusLabel->getFullText());
        video->setProgress(this->getProgress());

        if (this->hintBox->getVisibility() == brls::Visibility::VISIBLE)
            video->showHint(this->hintLabel->getFullText());

        video->showOSD(this->osd_state != OSDState::ALWAYS_ON);
        video->setFullscreenIcon(true);
        video->setHideHighlight(true);
        video->showReplay    = showReplay;
        video->real_duration = real_duration;
        video->setLastPlayedPosition(lastPlayedPosition);
        if (video->osdSlider && osdSlider) {
#ifndef __ANDROID__
            video->osdSlider->setClipPoint(osdSlider->getClipPoint());
#endif
        }
        video->refreshToggleIcon();
        video->setHighlightProgress(highlightData);
        if (video->isLiveMode) video->setLiveMode();
        video->setCustomToggleAction(customToggleAction);

        container->addView(video);
        auto activity = new brls::Activity(container);
        brls::Application::pushActivity(activity, brls::TransitionAnimation::NONE);
        registerFullscreen(activity);
    } else {
        // Pop fullscreen videoView
        brls::Application::popActivity(brls::TransitionAnimation::NONE);
    }
}

brls::View* VideoView::getDefaultFocus() {
    if (isFullscreen() && isOSDShown())
        return this->btnToggle;
    else
        return this;
}

brls::View* VideoView::getNextFocus(brls::FocusDirection direction, View* currentView) {
    if (this->isFullscreen()) return this;
    return brls::Box::getNextFocus(direction, currentView);
}

void VideoView::buttonProcessing() {
    brls::ControllerState state{};
    input->updateUnifiedControllerState(&state);

    if (isOSDShown() && (state.buttons[brls::BUTTON_NAV_RIGHT] || state.buttons[brls::BUTTON_NAV_LEFT] ||
                         state.buttons[brls::BUTTON_NAV_UP] || state.buttons[brls::BUTTON_NAV_DOWN])) {
        if (this->osd_state == OSDState::SHOWN) this->showOSD(true);
    }
    if (is_osd_lock) return;
}

void VideoView::registerMpvEvent() {
    if (registerMPVEvent) {
        brls::Logger::error("VideoView already register MPV Event");
    }
    eventSubscribeID = mpvCore->getEvent()->subscribe([this](MpvEventEnum event) {
        switch (event) {
            case MpvEventEnum::MPV_IDLE:
                refreshToggleIcon();
                break;
            case MpvEventEnum::MPV_RESUME:
                this->showReplay = false;
                this->showOSD(true);
                this->hideLoading();
                break;
            case MpvEventEnum::MPV_PAUSE:
                this->showOSD(false);
                break;
            case MpvEventEnum::START_FILE:
                this->showOSD(false);
                break;
            case MpvEventEnum::LOADING_START:
                this->showLoading();
                break;
            case MpvEventEnum::LOADING_END:
                this->hideLoading();
                break;
            case MpvEventEnum::MPV_STOP:
                this->hideLoading();
                this->showOSD(false);
                break;

            case MpvEventEnum::MPV_FILE_ERROR: {
                this->hideLoading();
                this->showOSD(false);
                // Mostra messaggio di errore diverso per live e video
                std::string errorKey = this->isLiveMode ? "hints/live_error"_i18n : "hints/video_error"_i18n;
                auto dialog = new brls::Dialog(errorKey);
                dialog->addButton("hints/back"_i18n,
                                  []() { brls::Application::popActivity(brls::TransitionAnimation::NONE); });
                dialog->open();
                break;
            }
            case MpvEventEnum::MPV_LOADED:
                this->setPlaybackTime(tsvitch::sec2Time(this->mpvCore->video_progress));
                if (lastPlayedPosition <= 0) break;
                if (abs(getRealDuration() - lastPlayedPosition) <= 5) {
                    mpvCore->seek(0);
                } else {
                    mpvCore->seek(lastPlayedPosition);
                    brls::Logger::info("Restore video position: {}", lastPlayedPosition);
                }
                lastPlayedPosition = 0;
                break;
            case MpvEventEnum::UPDATE_DURATION:
                this->setDuration(tsvitch::sec2Time(getRealDuration()));
                this->setProgress((float)mpvCore->playback_time / getRealDuration());
                break;
            case MpvEventEnum::UPDATE_PROGRESS:
                this->setPlaybackTime(tsvitch::sec2Time(this->mpvCore->video_progress));
                this->setProgress((float)mpvCore->playback_time / getRealDuration());
                break;
            case MpvEventEnum::END_OF_FILE:

                this->showOSD(false);
                if (EXIT_FULLSCREEN_ON_END && closeOnEndOfFile && this->isFullscreen()) {
                    this->setFullScreen(false);
                }

                if (this->onEndCallback) {
                    this->onEndCallback();
                }
                break;
            case MpvEventEnum::CACHE_SPEED_CHANGE:

                if (this->osdCenterBox->getVisibility() != brls::Visibility::GONE) {
                    if (this->centerLabel->getVisibility() != brls::Visibility::VISIBLE)
                        this->centerLabel->setVisibility(brls::Visibility::VISIBLE);
                    this->centerLabel->setText(mpvCore->getCacheSpeed());
                }
                break;
            case MpvEventEnum::VIDEO_MUTE:
                if (btnVolumeIcon) this->btnVolumeIcon->setImageFromSVGRes("svg/bpx-svg-sprite-volume-off.svg");
                break;
            case MpvEventEnum::VIDEO_UNMUTE:
                if (btnVolumeIcon) this->btnVolumeIcon->setImageFromSVGRes("svg/bpx-svg-sprite-volume.svg");
                break;
            case MpvEventEnum::RESET:
#ifndef __ANDROID__
                if (osdSlider) osdSlider->clearClipPoint();
#endif
                real_duration = 0;
                break;
            default:
                break;
        }
    });
    registerMPVEvent = true;
}

void VideoView::unRegisterMpvEvent() {
    if (!registerMPVEvent) return;
    mpvCore->getEvent()->unsubscribe(eventSubscribeID);
    registerMPVEvent = false;
}

void VideoView::onChildFocusGained(View* directChild, View* focusedView) {
    Box::onChildFocusGained(directChild, focusedView);
    if (is_osd_lock) {
        brls::Application::giveFocus(this->osdLockBox);
        return;
    }

    if (this->isFullscreen() && isOSDShown()) {
        if (focusedView->getParent()->getVisibility() == brls::Visibility::GONE) {
            brls::Application::giveFocus(this);
        }
        static View* lastFocusedView = nullptr;

        if (focusedView == this->btnSettingIcon) {
            this->btnSettingIcon->setCustomNavigationRoute(
                brls::FocusDirection::DOWN,
                lastFocusedView == this->btnToggle ? "video/osd/toggle" : "video/osd/lock/box");
        }
        lastFocusedView = focusedView;
        return;
    }
    brls::Application::giveFocus(this);
}

float VideoView::getRealDuration() { return real_duration > 0 ? (float)real_duration : (float)mpvCore->duration; }

void VideoView::setOnEndCallback(std::function<void()> callback) { this->onEndCallback = callback; }