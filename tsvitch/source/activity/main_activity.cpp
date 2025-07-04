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
#include "view/custom_button.hpp"
#include "view/auto_tab_frame.hpp"
#include "view/svg_image.hpp"

MainActivity::~MainActivity() { brls::Logger::debug("del MainActivity"); }

void MainActivity::onContentAvailable() {
    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_BACK,
        [this](brls::View* view) -> bool {
            Intent::openSetting([this]() {
                //if not focused
                if (!this->settingBtn->isFocused()) {
                    this->resetSettingIcon();
                }
            });
            return true;
        },
        true);

    this->registerAction(
        "Settings", brls::ControllerButton::BUTTON_START,
        [this](brls::View* view) -> bool {
            Intent::openSetting([this]() {
                //if not focused
                if (!this->settingBtn->isFocused()) {
                    this->resetSettingIcon();
                }
            });
            return true;
        },
        true);

    this->settingBtn->registerClickAction([this](brls::View* view) -> bool {
        Intent::openSetting([this]() {
            //if not focused
            if (!this->settingBtn->isFocused()) {
                this->resetSettingIcon();
            }
        });
        return true;
    });

    this->settingBtn->getFocusEvent()->subscribe([this](bool value) {
        SVGImage* image = dynamic_cast<SVGImage*>(this->settingBtn->getChildren()[0]);
        if (!image) return;
        if (value) {
            image->setImageFromSVGRes("svg/ico-setting-activate.svg");
            //wait
        } else {
            image->setImageFromSVGRes("svg/ico-setting.svg");
        }
    });

    this->settingBtn->setCustomNavigation([this](brls::FocusDirection direction) {
        if (tabFrame->getSideBarPosition() == AutoTabBarPosition::LEFT) {
            if (direction == brls::FocusDirection::RIGHT) {
                return (brls::View*)this->tabFrame->getActiveTab();
            }
        } else if (tabFrame->getSideBarPosition() == AutoTabBarPosition::TOP) {
            if (direction == brls::FocusDirection::DOWN) {
                return (brls::View*)this->tabFrame->getActiveTab();
            }
        }
        return (brls::View*)nullptr;
    });
    this->settingBtn->addGestureRecognizer(new brls::TapGestureRecognizer(this->settingBtn));
}

void MainActivity::resetSettingIcon() {
    SVGImage* image = dynamic_cast<SVGImage*>(this->settingBtn->getChildren()[0]);
    if (!image) return;

    image->setImageFromSVGRes("svg/ico-setting.svg");
}