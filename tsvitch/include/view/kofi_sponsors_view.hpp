#pragma once

#include <borealis/core/box.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

// Displays the Ko-fi supporters list.
//
// Ko-fi does not expose a public "list supporters" API (its API is
// webhook/push only), so the list is fetched from a small JSON feed that
// the maintainer publishes and updates manually (see docs/kofi_sponsors.json,
// published to GitHub Pages by .github/workflows/update-downloads-badge.yml).
// If the feed is unreachable or empty, a fallback link/QR to the Ko-fi page
// is shown instead (handled by the caller via the "sponsor_qr" section).
class KofiSponsorsView : public brls::Box {
public:
    KofiSponsorsView();

    static brls::View* create();

    void setOwner(const std::string& owner);
    void setFeedUrl(const std::string& url);

    void onLayout() override;

private:
    void loadSponsors();
    void renderList(const nlohmann::json& arr);
    void renderFallback();

    std::string owner   = "giovannimirulla";
    std::string feedUrl = "https://giovannimirulla.github.io/TsVitch/kofi_sponsors.json";
    bool loaded          = false;
};
