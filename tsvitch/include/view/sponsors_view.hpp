#pragma once

#include <borealis/core/box.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

class SponsorsView : public brls::Box {
public:
    SponsorsView();

    static brls::View* create();

    void setOwner(const std::string& owner);

    void onLayout() override;

private:
    void loadSponsors();
    void renderList(const nlohmann::json& arr);
    void renderFallback();

    std::string owner = "giovannimirulla";
    std::string feedUrl = "https://giovannimirulla.github.io/TsVitch/kofi_sponsors.json";
    bool loaded       = false;
};
