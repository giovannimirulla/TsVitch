

#pragma once

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>

class CustomButton;
class AutoTabFrame;
class AutoSidebarItem;

class MainActivity : public brls::Activity {
public:
    MainActivity();

    CONTENT_FROM_XML_RES("activity/main.xml");

    void onContentAvailable() override;
    void willAppear(bool resetState) override;

    void resetSettingIcon();

    ~MainActivity() override;
    

private:
    BRLS_BIND(AutoSidebarItem, settingBtn, "main/setting");

    BRLS_BIND(AutoTabFrame, tabFrame, "main/tabFrame");
    
    bool isReturningFromSettings = false;  // Flag per evitare loop infinito
    bool skipNextSettingsFocus = false;    // Salta il prossimo focus sul tab settings
};
