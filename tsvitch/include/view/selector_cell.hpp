

#pragma once

#include <borealis/views/cells/cell_selector.hpp>
#include "view/grid_dropdown.hpp"

class TsVitchSelectorCell : public brls::SelectorCell {
public:
    TsVitchSelectorCell() {
        detail->setTextColor(brls::Application::getTheme()["brls/list/listItem_value_color"]);

        this->registerClickAction([this](View* view) {
            brls::Logger::info("TsVitchSelectorCell: click title='{}' selection={} options={}",
                               this->title->getFullText(), selection, data.size());
            BaseDropdown::text(
                this->title->getFullText(), data, [this](int selected) {
                    brls::Logger::info("TsVitchSelectorCell: dropdown selected {}", selected);
                    this->setSelection(selected);
                }, selection);
            return true;
        });
    }

    static View* create() { return new TsVitchSelectorCell(); }
};
