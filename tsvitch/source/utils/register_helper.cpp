

#include "fragment/home_tab.hpp"
#include "fragment/home_live.hpp"
#include "fragment/home_history.hpp"

#include "utils/config_helper.hpp"

#include "view/auto_tab_frame.hpp"
#include "view/video_view.hpp"
#include "view/user_info.hpp"
#include "view/text_box.hpp"
#include "view/qr_image.hpp"
#include "view/svg_image.hpp"

#include "view/recycling_grid.hpp"
#include "view/grid_dropdown.hpp"

#include "view/video_progress_slider.hpp"
#include "view/gallery_view.hpp"
#include "view/custom_button.hpp"
#include "view/animation_image.hpp"
#include "view/button_close.hpp"
#include "view/check_box.hpp"
#include "view/video_profile.hpp"
#include "view/selector_cell.hpp"
#include "view/hint_label.hpp"
#include "view/mpv_core.hpp"

void Register::initCustomView() {
    brls::Application::registerXMLView("AutoTabFrame", AutoTabFrame::create);
    brls::Application::registerXMLView("RecyclingGrid", RecyclingGrid::create);
    brls::Application::registerXMLView("VideoView", VideoView::create);
    brls::Application::registerXMLView("VideoProfile", VideoProfile::create);
    brls::Application::registerXMLView("QRImage", QRImage::create);
    brls::Application::registerXMLView("SVGImage", SVGImage::create);
    brls::Application::registerXMLView("TextBox", TextBox::create);
    brls::Application::registerXMLView("VideoProgressSlider", VideoProgressSlider::create);
    brls::Application::registerXMLView("GalleryView", GalleryView::create);
    brls::Application::registerXMLView("CustomButton", CustomButton::create);
    brls::Application::registerXMLView("HintLabel", HintLabel::create);

    brls::Application::registerXMLView("UserInfoView", UserInfoView::create);

    brls::Application::registerXMLView("ButtonClose", ButtonClose::create);
    brls::Application::registerXMLView("CheckBox", BiliCheckBox::create);
    brls::Application::registerXMLView("SelectorCell", BiliSelectorCell::create);
    brls::Application::registerXMLView("AnimationImage", AnimationImage::create);

    //     Register fragments
    brls::Application::registerXMLView("HomeTab", HomeTab::create);

    brls::Application::registerXMLView("HomeLive", HomeLive::create);
    brls::Application::registerXMLView("HomeHistory", HomeHistory::create);
}

void Register::initCustomTheme() {
    brls::Theme::getLightTheme().addColor("color/grey_1", nvgRGB(245, 246, 247));
    brls::Theme::getDarkTheme().addColor("color/grey_1", nvgRGB(51, 52, 53));

    brls::Theme::getLightTheme().addColor("color/grey_2", nvgRGB(245, 245, 245));
    brls::Theme::getDarkTheme().addColor("color/grey_2", nvgRGB(51, 53, 55));

    brls::Theme::getLightTheme().addColor("color/grey_3", nvgRGBA(200, 200, 200, 16));
    brls::Theme::getDarkTheme().addColor("color/grey_3", nvgRGBA(160, 160, 160, 160));

    brls::Theme::getLightTheme().addColor("color/grey_4", nvgRGBA(48, 48, 48, 10));
    brls::Theme::getDarkTheme().addColor("color/grey_4", nvgRGBA(48, 48, 48, 10));

    brls::Theme::getLightTheme().addColor("font/grey", nvgRGB(148, 153, 160));
    brls::Theme::getDarkTheme().addColor("font/grey", nvgRGB(148, 153, 160));

    brls::Theme::getLightTheme().addColor("color/yellow_1", nvgRGB(253, 246, 230));
    brls::Theme::getDarkTheme().addColor("color/yellow_1", nvgRGB(46, 33, 17));

    brls::Theme::getLightTheme().addColor("font/yellow_1", nvgRGB(217, 118, 7));
    brls::Theme::getDarkTheme().addColor("font/yellow_1", nvgRGB(217, 118, 7));

    brls::Theme::getLightTheme().addColor("color/tsvitch", nvgRGB(255, 145, 0));
    brls::Theme::getDarkTheme().addColor("color/tsvitch", nvgRGB(255, 145, 0));

    brls::Theme::getLightTheme().addColor("color/link", nvgRGB(102, 147, 182));
    brls::Theme::getDarkTheme().addColor("color/link", nvgRGB(102, 147, 182));

    brls::Theme::getLightTheme().addColor("color/line", nvgRGB(208, 208, 208));
    brls::Theme::getDarkTheme().addColor("color/line", nvgRGB(100, 100, 100));

    brls::Theme::getLightTheme().addColor("color/pink_1", nvgRGB(252, 237, 241));
    brls::Theme::getDarkTheme().addColor("color/pink_1", nvgRGB(44, 27, 34));

    brls::Theme::getLightTheme().addColor("color/tip/red", nvgRGB(250, 88, 87));
    brls::Theme::getDarkTheme().addColor("color/tip/red", nvgRGB(211, 63, 64));

    brls::Theme::getLightTheme().addColor("color/white", nvgRGB(255, 255, 255));
    brls::Theme::getDarkTheme().addColor("color/white", nvgRGBA(255, 255, 255, 180));

    brls::Theme::getLightTheme().addColor("captioned_image/caption", nvgRGB(2, 176, 183));
    brls::Theme::getDarkTheme().addColor("captioned_image/caption", nvgRGB(51, 186, 227));
}

void Register::initCustomStyle() {
    brls::getStyle().addMetric("brls/animations/highlight", 200);

    if (brls::Application::ORIGINAL_WINDOW_HEIGHT == 544) {
        brls::getStyle().addMetric("tsvitch/grid/span/5", 4);
        brls::getStyle().addMetric("tsvitch/grid/span/4", 3);
        brls::getStyle().addMetric("tsvitch/grid/span/3", 2);
        brls::getStyle().addMetric("tsvitch/grid/span/2", 2);
        brls::getStyle().addMetric("tsvitch/player/width", 600);
        brls::getStyle().addMetric("tsvitch/player/bottom/font", 10);
        brls::getStyle().addMetric("tsvitch/comment/level/x", 30);
        brls::getStyle().addMetric("tsvitch/margin/20", 10);
        brls::getStyle().addMetric("tsvitch/about/qr", 150);
        brls::getStyle().addMetric("tsvitch/about/speech/width", 420);
        brls::getStyle().addMetric("tsvitch/about/speech/header", 495);
        brls::getStyle().addMetric("tsvitch/tab_frame/content_padding_top_bottom", 20);
        brls::getStyle().addMetric("tsvitch/mine/num", 18);
        brls::getStyle().addMetric("tsvitch/mine/type", 12);
        brls::getStyle().addMetric("tsvitch/setting/about/bottom", 0);
        brls::getStyle().addMetric("tsvitch/dynamic/video/card/padding", 10);
        brls::getStyle().addMetric("brls/tab_frame/content_padding_sides", 30);
    } else {
        switch (brls::Application::ORIGINAL_WINDOW_HEIGHT) {
            case 1080:
                brls::getStyle().addMetric("tsvitch/grid/span/5", 7);
                brls::getStyle().addMetric("tsvitch/grid/span/4", 6);
                brls::getStyle().addMetric("tsvitch/grid/span/3", 5);
                brls::getStyle().addMetric("tsvitch/grid/span/2", 4);
                break;
            case 900:
                brls::getStyle().addMetric("tsvitch/grid/span/5", 6);
                brls::getStyle().addMetric("tsvitch/grid/span/4", 5);
                brls::getStyle().addMetric("tsvitch/grid/span/3", 4);
                brls::getStyle().addMetric("tsvitch/grid/span/2", 3);
                break;
            case 720:
            default:
                brls::getStyle().addMetric("tsvitch/grid/span/5", 5);
                brls::getStyle().addMetric("tsvitch/grid/span/4", 4);
                brls::getStyle().addMetric("tsvitch/grid/span/3", 3);
                brls::getStyle().addMetric("tsvitch/grid/span/2", 2);
        }
        brls::getStyle().addMetric("tsvitch/player/width", brls::Application::ORIGINAL_WINDOW_WIDTH - 480);
        brls::getStyle().addMetric("tsvitch/player/bottom/font", 13);
        brls::getStyle().addMetric("tsvitch/comment/level/x", 45);
        brls::getStyle().addMetric("tsvitch/margin/20", 20);
        brls::getStyle().addMetric("tsvitch/about/qr", 200);
        brls::getStyle().addMetric("tsvitch/about/speech/width", 660);
        brls::getStyle().addMetric("tsvitch/about/speech/header", 760);
        brls::getStyle().addMetric("tsvitch/tab_frame/content_padding_top_bottom", 42);
        brls::getStyle().addMetric("tsvitch/mine/num", 24);
        brls::getStyle().addMetric("tsvitch/mine/type", 16);
        brls::getStyle().addMetric("tsvitch/setting/about/bottom", 50);
        brls::getStyle().addMetric("tsvitch/dynamic/video/card/padding", 20);
    }
}
