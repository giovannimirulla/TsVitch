#include "view/sponsors_view.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/image.hpp>
#include <cstdlib>
#include <fstream>
#include <chrono>
#include "utils/config_helper.hpp"
#include "utils/image_helper.hpp"

using nlohmann::json;

SponsorsView::SponsorsView() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(0);
    this->setWidth(brls::View::AUTO);

    this->registerStringXMLAttribute("owner", [this](const std::string& value) { this->setOwner(value); });
}

brls::View* SponsorsView::create() { return new SponsorsView(); }

void SponsorsView::setOwner(const std::string& o) { this->owner = o; }

void SponsorsView::onLayout() {
    brls::Box::onLayout();
    if (!loaded) {
        loaded = true;
        loadSponsors();
    }
}

void SponsorsView::loadSponsors() {
    // Cache path
    std::string cachePath = ProgramConfig::instance().getConfigDir() + "/sponsors_cache.json";
    bool usedCache        = false;
    
    // Try cache first (valid for 24h)
    try {
        std::ifstream in(cachePath);
        if (in.good()) {
            json cached;
            in >> cached;
            in.close();
            auto now     = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
            auto fetched = cached.value("fetched_at", (long long)0);
            if (now - fetched < 24 * 60 * 60 && cached.contains("data") && cached["data"].is_array()) {
                renderList(cached["data"]);
                usedCache = true;
            }
        }
    } catch (...) {
        // ignore cache errors
    }
    
    if (usedCache) return;

    const char* token = std::getenv("GITHUB_TOKEN");
    if (!token) {
        renderFallback();
        return;
    }

    try {
        std::string query = R"(query($login:String!){ user(login:$login){ sponsors(first:100){ nodes { login url } } } })";
        json body         = {
                    {"query", query},
                    {"variables", json{{"login", owner}}},
        };

        auto response = cpr::Post(cpr::Url{"https://api.github.com/graphql"},
                                   cpr::Header{{"Authorization", std::string("bearer ") + token},
                                               {"User-Agent", "TsVitch"},
                                               {"Accept", "application/json"}},
                                   cpr::Body{body.dump()},
                                   cpr::Timeout{6000});

        if (response.status_code != 200) {
            renderFallback();
            return;
        }

        auto j = json::parse(response.text, nullptr, false);
        if (j.is_discarded()) {
            renderFallback();
            return;
        }

        auto nodes = j["data"]["user"]["sponsors"]["nodes"];
        if (!nodes.is_array() || nodes.empty()) {
            renderFallback();
            return;
        }

        // Save cache
        try {
            json cached = {{"fetched_at", (long long)std::chrono::system_clock::to_time_t(
                                             std::chrono::system_clock::now())},
                           {"data", nodes}};
            std::ofstream out(cachePath);
            if (out.good()) {
                out << cached.dump();
                out.close();
            }
        } catch (...) {
        }

        renderList(nodes);
    } catch (...) {
        renderFallback();
    }
}

void SponsorsView::renderList(const json& arr) {
    // Create a grid container for sponsors with manual wrapping (3 columns)
    auto* mainContainer = new brls::Box();
    mainContainer->setAxis(brls::Axis::COLUMN);
    mainContainer->setGrow(0);
    mainContainer->setMarginLeft(10);
    mainContainer->setMarginRight(10);

    int colsPerRow = 3;
    int count = 0;
    auto* currentRow = new brls::Box();
    currentRow->setAxis(brls::Axis::ROW);
    currentRow->setGrow(0);
    currentRow->setJustifyContent(brls::JustifyContent::FLEX_START);
    currentRow->setAlignItems(brls::AlignItems::FLEX_START);

    for (const auto& node : arr) {
        if (!node.is_object()) continue;
        std::string login = node.value("login", "");
        std::string avatarUrl = node.value("avatarUrl", "");
        std::string url = node.value("url", "");

        // Create a card for each sponsor
        auto* card = new brls::Box();
        card->setAxis(brls::Axis::COLUMN);
        card->setGrow(0);
        card->setWidth(200);
        card->setMarginLeft(10);
        card->setMarginRight(10);
        card->setMarginTop(10);
        card->setMarginBottom(10);
        card->setAlignItems(brls::AlignItems::CENTER);
        card->setJustifyContent(brls::JustifyContent::CENTER);

        // Avatar image
        if (!avatarUrl.empty()) {
            auto* avatarImg = new brls::Image();
            avatarImg->setWidth(70);
            avatarImg->setHeight(70);
            avatarImg->setCornerRadius(35);
            ImageHelper::with(avatarImg)->load(avatarUrl);
            card->addView(avatarImg);
        }

        // Sponsor name (single line with truncation)
        auto* nameLabel = new brls::Label();
        nameLabel->setText(login);
        nameLabel->setMarginTop(6);
        nameLabel->setFontSize(13);
        nameLabel->setMarginLeft(2);
        nameLabel->setMarginRight(2);
        card->addView(nameLabel);

        // Make card clickable
        card->registerClickAction([url](...) -> bool {
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
#ifdef __linux__
            if (!brls::isSteamDeck())
#endif
            {
                auto* p = (brls::DesktopPlatform*)brls::Application::getPlatform();
                p->openBrowser(url);
            }
#endif
            return true;
        });

        currentRow->addView(card);
        count++;

        // Create new row after colsPerRow items
        if (count % colsPerRow == 0) {
            mainContainer->addView(currentRow);
            currentRow = new brls::Box();
            currentRow->setAxis(brls::Axis::ROW);
            currentRow->setGrow(0);
            currentRow->setJustifyContent(brls::JustifyContent::FLEX_START);
            currentRow->setAlignItems(brls::AlignItems::FLEX_START);
        }
    }

    // Add remaining items in last row if any
    if (count % colsPerRow != 0) {
        mainContainer->addView(currentRow);
    }

    if (count == 0) renderFallback();
    else this->addView(mainContainer);
}

void SponsorsView::renderFallback() {
    // Show a link to the sponsors page if listing cannot be retrieved
    std::string url = std::string("https://github.com/sponsors/") + owner;
    auto* label     = new brls::Label();
    label->setText("Diventa sponsor: " + url);
    label->setMarginLeft(20);
    label->registerClickAction([url](...) -> bool {
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
#ifdef __linux__
        if (!brls::isSteamDeck())
#endif
        {
            auto* p = (brls::DesktopPlatform*)brls::Application::getPlatform();
            p->openBrowser(url);
        }
#endif
        return true;
    });
    this->addView(label);
}
