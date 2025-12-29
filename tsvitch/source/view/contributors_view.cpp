#include "view/contributors_view.hpp"
#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/image.hpp>
#include <fstream>
#include <chrono>
#include "utils/config_helper.hpp"
#include "utils/image_helper.hpp"

using nlohmann::json;

ContributorsView::ContributorsView() {
    this->setAxis(brls::Axis::COLUMN);
    this->setGrow(0);
    this->setWidth(brls::View::AUTO);

    this->registerStringXMLAttribute("repo", [this](const std::string& value) { this->setRepo(value); });
    this->registerFloatXMLAttribute("limit", [this](float value) { this->setLimit((int)value); });
}

brls::View* ContributorsView::create() { return new ContributorsView(); }

void ContributorsView::setRepo(const std::string& r) { this->repo = r; }

void ContributorsView::setLimit(int l) { this->limit = l; }

void ContributorsView::onLayout() {
    brls::Box::onLayout();
    if (!loaded) {
        loaded = true;
        loadContributors();
    }
}

void ContributorsView::loadContributors() {
    // Show a temporary loading label
    auto* loading = new brls::Label();
    loading->setText("Caricamento contributori...");
    loading->setTextColor(brls::Application::getTheme()["font/grey"]);
    this->addView(loading);

    try {
        // Cache path
        std::string cachePath = ProgramConfig::instance().getConfigDir() + "/contributors_cache.json";
        bool usedCache         = false;
        
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
                    this->removeView(loading);
                    renderList(cached["data"]);
                    usedCache = true;
                }
            }
        } catch (...) {
            // ignore cache errors
        }

        if (usedCache) return;

        std::string url = "https://api.github.com/repos/" + repo + "/contributors?per_page=" + std::to_string(limit);
        auto response   = cpr::Get(cpr::Url{url},
                                 cpr::Header{{"Accept", "application/vnd.github+json"},
                                             {"User-Agent", "TsVitch"}},
                                 cpr::Timeout{5000});

        this->removeView(loading);

        if (response.status_code == 200) {
            auto arr = json::parse(response.text, nullptr, false);
            if (!arr.is_array()) {
                auto* err = new brls::Label();
                err->setText("Impossibile leggere la lista contributori");
                this->addView(err);
                return;
            }
            
            // Save cache
            try {
                json cached = {{"fetched_at", (long long)std::chrono::system_clock::to_time_t(
                                                 std::chrono::system_clock::now())},
                               {"data", arr}};
                std::ofstream out(cachePath);
                if (out.good()) {
                    out << cached.dump();
                    out.close();
                }
            } catch (...) {
            }
            
            renderList(arr);
        } else {
            auto* err = new brls::Label();
            err->setText("Errore GitHub: " + std::to_string(response.status_code));
            this->addView(err);
        }
    } catch (const std::exception& e) {
        this->removeView(loading);
        auto* err = new brls::Label();
        err->setText(std::string("Errore di rete: ") + e.what());
        this->addView(err);
    }
}

void ContributorsView::renderList(const json& arr) {
    // Create a grid container for contributors with manual wrapping (3 columns)
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

    for (const auto& item : arr) {
        if (limit > 0 && count >= limit) break;
        if (!item.is_object()) continue;
        
        std::string login = item.value("login", "");
        std::string avatar = item.value("avatar_url", "");
        int contrib = item.value("contributions", 0);
        std::string url = item.value("html_url", "");

        // Create a card for each contributor
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
        if (!avatar.empty()) {
            auto* avatarImg = new brls::Image();
            avatarImg->setWidth(70);
            avatarImg->setHeight(70);
            avatarImg->setCornerRadius(35);
            ImageHelper::with(avatarImg)->load(avatar);
            card->addView(avatarImg);
        }

        // Contributor name
        auto* nameLabel = new brls::Label();
        nameLabel->setText(login);
        nameLabel->setMarginTop(6);
        nameLabel->setFontSize(13);
        nameLabel->setMarginLeft(2);
        nameLabel->setMarginRight(2);
        card->addView(nameLabel);

        // Contribution count
        if (contrib > 0) {
            auto* countLabel = new brls::Label();
            countLabel->setText("+" + std::to_string(contrib));
            countLabel->setMarginTop(2);
            countLabel->setFontSize(11);
            countLabel->setTextColor(brls::Application::getTheme()["font/grey"]);
            card->addView(countLabel);
        }

        // Make card clickable
        card->registerClickAction([url](...) -> bool {
#if !defined(__SWITCH__) && !defined(__PSV__) && !defined(PS4)
#ifdef __linux__
            if (!brls::isSteamDeck())
#endif
            {
                auto* platform = brls::Application::getPlatform();
                if (platform) platform->openBrowser(url);
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

    if (count == 0) {
        auto* none = new brls::Label();
        none->setText("Nessun contributore trovato");
        mainContainer->addView(none);
    }

    this->addView(mainContainer);
}
