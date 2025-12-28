#pragma once

#include <borealis/core/box.hpp>
#include <cpr/cpr.h>
#include <nlohmann/json.hpp>

class ContributorsView : public brls::Box {
public:
    ContributorsView();

    static brls::View* create();

    void setRepo(const std::string& repo);
    void setLimit(int limit);

    void onLayout() override;

private:
    void loadContributors();
    void renderList(const nlohmann::json& arr);

    std::string repo = "giovannimirulla/TsVitch";
    int limit        = 30;
    bool loaded      = false;
};
