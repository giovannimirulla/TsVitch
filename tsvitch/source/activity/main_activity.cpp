/*
    Copyright 2020-2021 natinusala

    Licensed under the Apache License, Version 2.0 (the "License");
    you may not use this file except in compliance with the License.
    You may obtain a copy of the License at

        http://www.apache.org/licenses/LICENSE-2.0

    Unless required by applicable law or agreed to in writing, software
    distributed under the License is distributed on an "AS IS" BASIS,
    WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
    See the License for the specific language governing permissions and
    limitations under the License.
*/

#include <borealis/core/touch/tap_gesture.hpp>

#include "activity/main_activity.hpp"
#include "utils/activity_helper.hpp"
#include "utils/dialog_helper.hpp"
#include "utils/config_helper.hpp"
#include "view/custom_button.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/svg_image.hpp"

MainActivity::MainActivity() {
    brls::Logger::info("MainActivity constructor called");
}

MainActivity::~MainActivity() { brls::Logger::debug("del MainActivity"); }

void MainActivity::onContentAvailable() {
    brls::Logger::info("MainActivity::onContentAvailable() called");

    // Aggiungi uno spacer prima degli ultimi 4 tab (Preferiti, Download, History, Settings) per spingerli in basso
    if (this->tabFrame.getView()) {
        auto* sidebar = this->tabFrame->getSidebar();
        if (sidebar) {
            auto& children = sidebar->getChildren();
            // Lo spacer deve essere inserito dopo il 3° tab (HomeSeries) che ha indice 2
            if (children.size() > 3) {
                auto* spacer = new brls::Box();
                spacer->setGrow(1.0f);  // Occupa tutto lo spazio disponibile
                spacer->setVisibility(brls::Visibility::VISIBLE);
                // Inserisci lo spacer all'indice 3 (dopo Live, VOD, Series)
                sidebar->addView(spacer, 3);
                brls::Logger::info("MainActivity: added spacer at index 3");
            }
        }
    }

    // Nascondi i tab VOD e Serie quando si usa la sola playlist M3U8 (IPTV_MODE == 0)
    int iptvMode = ProgramConfig::instance().getSettingItem(SettingItem::IPTV_MODE, 0);
    brls::Logger::info("MainActivity: IPTV_MODE = {}", iptvMode);
    
    if (iptvMode == 0) {
        if (!this->tabFrame.getView()) {
            brls::Logger::warning("MainActivity: tabFrame not bound yet");
        } else {
            auto* sidebar = this->tabFrame->getSidebar();
            if (!sidebar) {
                brls::Logger::error("MainActivity: sidebar is null");
            } else {
                auto& children = sidebar->getChildren();
                brls::Logger::info("MainActivity: sidebar has {} children", children.size());
                
                // Nascondi VOD (indice 1) e Series (indice 2)
                if (children.size() > 1) {
                    brls::Logger::info("MainActivity: hiding VOD tab (index 1)");
                    children[1]->setVisibility(brls::Visibility::GONE);
                }
                if (children.size() > 2) {
                    brls::Logger::info("MainActivity: hiding Series tab (index 2)");
                    children[2]->setVisibility(brls::Visibility::GONE);
                }
                
                brls::Logger::info("MainActivity: VOD and Series tabs hidden for M3U8 mode");
            }
        }
    } else {
        brls::Logger::info("MainActivity: Xtream mode active, showing all tabs");
    }
    
    // Controlla la connettività internet
    bool hasInternet = brls::Application::getPlatform()->hasEthernetConnection() || 
                      brls::Application::getPlatform()->hasWirelessConnection();
    
    if (!hasInternet) {
        brls::Logger::info("No internet connection detected, navigating to Downloads tab");
        // Se non c'è internet, vai direttamente al tab Downloads (indice 2)
        if (this->tabFrame) {
            this->tabFrame->focusTab(2); // Assumendo che Downloads sia il 3° tab (indice 2)
        }
    }
    
    // Intercetta il tab Settings per aprire le impostazioni
    if (this->settingBtn.getView()) {
        brls::Logger::info("MainActivity: setting up Settings tab");
        
        // Quando il tab Settings riceve il focus, apri immediatamente le impostazioni
        this->settingBtn->getFocusEvent()->subscribe([this](bool focused) {
            if (focused) {
                // Se dobbiamo saltare il prossimo focus (dopo la chiusura delle impostazioni)
                if (this->skipNextSettingsFocus) {
                    brls::Logger::info("MainActivity: skipping focus after settings close");
                    this->skipNextSettingsFocus = false;
                    // Forza focus su Home per ripristinare contenuto e stato attivo
                    if (this->tabFrame.getView()) {
                        auto* home     = this->tabFrame->getItem(0);
                        auto* settings = this->settingBtn.getView();

                        if (home) {
                            this->tabFrame->focusTab(0);  // trigga onFocusGained e setActive del gruppo
                            home->setActive(true);
                        }
                        if (settings) settings->setActive(false);

                        this->tabFrame->invalidate();
                    }
                    return;
                }
                
                brls::Logger::info("MainActivity: Settings tab focused, opening settings");
                
                // Imposta il flag prima di aprire le impostazioni
                this->isReturningFromSettings = true;
                
                // Apri le impostazioni
                Intent::openSettings([this]() {
                    brls::Logger::info("Settings closed");
                    // Alla chiusura, salta il prossimo focus su Settings e porta il focus su Home
                    this->skipNextSettingsFocus = true;
                    if (this->tabFrame.getView()) {
                        auto* home     = this->tabFrame->getItem(0);
                        auto* settings = this->settingBtn.getView();

                        if (home) {
                            this->tabFrame->focusTab(0);  // trigga onFocusGained e setActive del gruppo
                            home->setActive(true);
                        }
                        if (settings) settings->setActive(false);

                        this->tabFrame->invalidate();
                    }
                });
            }
        });
    }
    
    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_BACK,
        [this](brls::View* view) -> bool {
            this->isReturningFromSettings = true;
            Intent::openSettings([this]() { this->skipNextSettingsFocus = true; });
            return true;
        },
        true);

    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_START,
        [this](brls::View* view) -> bool {
            this->isReturningFromSettings = true;
            Intent::openSettings([this]() { this->skipNextSettingsFocus = true; });
            return true;
        },
        true);
}

void MainActivity::resetSettingIcon() {
    // No longer needed since Settings is now a regular tab
}

void MainActivity::willAppear(bool resetState) {
    brls::Activity::willAppear(resetState);

    if (this->isReturningFromSettings && this->tabFrame.getView()) {
        auto* home     = this->tabFrame->getItem(0);
        auto* settings = this->settingBtn.getView();

        if (home) {
            this->tabFrame->focusTab(0);
            home->setActive(true);
        }
        if (settings) settings->setActive(false);

        this->tabFrame->invalidate();
    }

    this->isReturningFromSettings = false;
    this->skipNextSettingsFocus   = false;
}