

#include <pystring.h>
#include <regex>
#include <borealis/core/application.hpp>
#include <borealis/views/rectangle.hpp>
#include <borealis/views/image.hpp>

#include "fragment/latest_update.hpp"
#include "analytics.h"
#include "utils/config_helper.hpp"
#include "utils/image_helper.hpp"
#include "view/text_box.hpp"

static inline bool isSeparator(const std::string& text) {
    return std::all_of(text.begin(), text.end(), [](char c) { return c == ' ' || c == '-'; });
}

static inline bool isListItem(const std::string& text) {
    return pystring::startswith(pystring::lstrip(text, " "), "-");
}

static inline int getStartSpace(const std::string& text) { return pystring::find(text, "-"); }

static inline bool isHeader(const std::string& text) { return pystring::startswith(text, "#"); }

static inline size_t getHeaderSize(const std::string& text) {
    size_t res = text.find_first_not_of('#');
    if (res < 1) return 1;
    if (res > 5) return 5;
    return res;
}

static inline bool isImage(const std::string& text) { return pystring::startswith(pystring::lstrip(text), "!["); }

#define PARSE_IMAGE_DATA(index, name, def)               \
    if (data.size() > (index) && !data[index].empty()) { \
        try {                                            \
            (name) = std::stoi(data[index]);             \
        } catch (const std::invalid_argument& e) {       \
            (name) = def;                                \
        }                                                \
    } else {                                             \
        (name) = def;                                    \
    }
static inline std::string getImageUrl(const std::string& test, int& maxHeight, int& margin) {
    std::regex re(R"(\!\[(.*)\]\((.*)\))");
    std::smatch match;
    if (std::regex_search(test, match, re)) {
        auto data = pystring::split(match[1], ",");
        PARSE_IMAGE_DATA(0, maxHeight, 200)
        PARSE_IMAGE_DATA(1, margin, 10)
        return match[2];
    }
    return "";
}

#define SHOW_REACTION(emoji, count)          \
    if ((count) > 0) {                       \
        auto reaction = new brls::Label();   \
        auto num      = new brls::Label();   \
        reaction->setText(emoji);            \
        num->setText(std::to_string(count)); \
        num->setMarginLeft(10);              \
        num->setMarginRight(20);             \
        topBox->addView(reaction);           \
        topBox->addView(num);                \
    }

LatestUpdate::LatestUpdate(const ReleaseNote& info) {
    this->inflateFromXMLRes("xml/fragment/latest_update.xml");
    brls::Logger::debug("Fragment LatestUpdate: create");

    header->setText(info.name);
    subtitle->setText(fmt::format("A new version has been released, your current version is: {}", APPVersion::instance().git_tag));  author->setUserInfo(info.author.avatar_url, info.author.login, info.published_at);

    SHOW_REACTION("👍", info.reactions.plus_one);
    SHOW_REACTION("😀", info.reactions.laugh);
    SHOW_REACTION("🎉", info.reactions.hooray);
    SHOW_REACTION("❤️", info.reactions.heart);
    SHOW_REACTION("🚀", info.reactions.rocket);
    SHOW_REACTION("👀", info.reactions.eyes);

    auto theme     = brls::Application::getTheme();
    auto lineColor = theme.getColor("brls/applet_frame/separator");
    auto content   = pystring::replace(info.body, "\r\n", "\n");
    auto lines     = pystring::split(content, "\n");
    for (auto& l : lines) {
        if (l.empty()) {
            auto space = new brls::Rectangle(nvgRGBA(0, 0, 0, 0));
            space->setHeight(20);
            textbox->addView(space);
        } else if (isSeparator(l)) {
            auto space = new brls::Rectangle(lineColor);
            space->setHeight(1);
            textbox->addView(space);
        } else if (isListItem(l)) {
            auto linePoint = new brls::Label();
            linePoint->setText("•");
            linePoint->setMarginRight(10);
            linePoint->setVerticalAlign(brls::VerticalAlign::TOP);

            auto line = new TextBox();
            int start = getStartSpace(l);
            line->setText(l.substr(start + 1));
            line->setMarginBottom(10);
            line->setGrow(1);

            auto lineBox = new brls::Box();
            lineBox->setMarginLeft((float)start * 10);
            lineBox->addView(linePoint);
            lineBox->addView(line);
            textbox->addView(lineBox);
        } else if (isImage(l)) {
            int maxHeight = 0, margin = 0;
            auto url = getImageUrl(l, maxHeight, margin);
            if (url.empty()) continue;
            auto box   = new brls::Box();
            auto image = new brls::Image();
            image->setMaxHeight((float)maxHeight);
            image->setMargins((float)margin, (float)margin, (float)margin, (float)margin);
            ImageHelper::with(image)->load(url);
            box->addView(image);
            box->setJustifyContent(brls::JustifyContent::CENTER);
            textbox->addView(box);
        } else {
            auto line = new brls::Label();
            line->setMarginBottom(10);
            if (isHeader(l)) {
                size_t size = getHeaderSize(l);
                line->setFontSize(48 - 6 * (float)size);
                line->setMarginBottom(20 - 2 * (float)size);
                line->setText(pystring::lstrip(l, "# "));
            } else {
                line->setText(l);
            }
            textbox->addView(line);
        }
    }

    GA("show_update_dialog")
}

LatestUpdate::~LatestUpdate() { brls::Logger::debug("Fragment LatestUpdate: delete"); }
