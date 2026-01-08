#include "fragment/home_vod.hpp"

#include <borealis/core/logger.hpp>
#include <borealis/views/dialog.hpp>
#include <borealis/core/thread.hpp>

#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "utils/activity_helper.hpp"
#include "utils/image_helper.hpp"
#include "api/tsvitch/result/home_live_result.h"
#include "utils/xtream_helper.hpp"

using namespace brls::literals;

class VodCategoryCell : public RecyclingGridItem {
public:
    VodCategoryCell() {
        this->inflateFromXMLRes("xml/views/group_channel_dynamic.xml");
        auto theme    = brls::Application::getTheme();
        selectedColor = theme.getColor("color/tsvitch");
        fontColor     = theme.getColor("brls/text");
    }

    void setTitle(const std::string& title) { this->labelTitle->setText(title); }
    void setSelected(bool selected) { this->labelTitle->setTextColor(selected ? selectedColor : fontColor); }

    void prepareForReuse() override {
        this->labelTitle->setText("");
        this->labelTitle->setTextColor(fontColor);
    }

    void cacheForReuse() override {}

    static RecyclingGridItem* create() { return new VodCategoryCell(); }

private:
    BRLS_BIND(brls::Label, labelTitle, "title");
    NVGcolor selectedColor{};
    NVGcolor fontColor{};
};

class VodCategoryDataSource : public RecyclingGridDataSource {
public:
    using OnCategorySelected = std::function<void(size_t)>;

    VodCategoryDataSource(std::vector<std::string> names, OnCategorySelected cb)
        : list(std::move(names)), onCategorySelected(std::move(cb)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = static_cast<VodCategoryCell*>(recycler->dequeueReusableCell("Cell"));
        item->setTitle(this->list[index]);
        item->setSelected(index == selectedIndex);
        return item;
    }

    size_t getItemCount() override { return list.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= list.size()) return;

        std::vector<RecyclingGridItem*>& items = recycler->getGridItems();
        for (auto& i : items) {
            auto* cell = dynamic_cast<VodCategoryCell*>(i);
            if (cell) cell->setSelected(false);
        }

        selectedIndex = index;
        auto* item    = dynamic_cast<VodCategoryCell*>(recycler->getGridItemByIndex(index));
        if (item) item->setSelected(true);

        if (onCategorySelected) onCategorySelected(index);
    }

    void clearData() override { list.clear(); }

    void setSelectedIndex(RecyclingGrid* recycler, size_t index) {
        if (index >= list.size()) return;
        selectedIndex = index;
        auto* item    = dynamic_cast<VodCategoryCell*>(recycler->getGridItemByIndex(index));
        if (item) item->setSelected(true);
    }

private:
    std::vector<std::string> list;
    size_t selectedIndex = 0;
    OnCategorySelected onCategorySelected;
};

class VodStreamDataSource : public RecyclingGridDataSource {
public:
    explicit VodStreamDataSource(tsvitch::LiveM3u8ListResult list) : liveList(std::move(list)) {}

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        auto* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");
        item->setChannel(liveList[index]);
        return item;
    }

    size_t getItemCount() override { return liveList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index >= liveList.size()) return;
        Intent::openLive(liveList, index, [recycler]() { recycler->reloadData(); });
    }

    void clearData() override { liveList.clear(); }

private:
    tsvitch::LiveM3u8ListResult liveList;
};

HomeVOD::HomeVOD() {
    this->inflateFromXMLRes("xml/fragment/home_vod.xml");

    categoryGrid->registerCell("Cell", []() { return VodCategoryCell::create(); });
    streamGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });
}

HomeVOD::~HomeVOD() { brls::Logger::debug("HomeVOD destroyed"); }

void HomeVOD::onCreate() {
    brls::Logger::info("HomeVOD::onCreate");
    streamGrid->showSkeleton();
    categoryGrid->showSkeleton();
    requestVODCategories();
}

void HomeVOD::onShow() {
    brls::Logger::debug("HomeVOD::onShow");
}

void HomeVOD::onVODCategories(const std::vector<tsvitch::XtreamVODCategory>& categories) {
    vodCategories = categories;
    buildCategoryList(categories);
}

void HomeVOD::onVODStreams(const std::vector<tsvitch::XtreamVODStream>& streams, const std::string& categoryName) {
    vodStreams = streams;
    buildStreams(streams, categoryName);
}

void HomeVOD::onError(const std::string& error) {
    brls::Logger::error("HomeVOD error: {}", error);
    auto* dialog = new brls::Dialog(error);
    dialog->addButton("OK", []() {});
    dialog->open();
}

void HomeVOD::buildCategoryList(const std::vector<tsvitch::XtreamVODCategory>& categories) {
    std::vector<std::string> names;
    names.reserve(categories.size());
    for (const auto& c : categories) {
        names.push_back(c.category_name);
    }

    categoryDataSource = std::make_unique<VodCategoryDataSource>(names, [this](size_t index) { selectCategory(index); });
    categoryGrid->setDataSource(categoryDataSource.get());

    if (!categories.empty()) {
        categoryDataSource->setSelectedIndex(categoryGrid, 0);
        selectCategory(0);
    }
}

void HomeVOD::selectCategory(size_t index) {
    if (index >= vodCategories.size()) return;
    selectedCategoryIndex = index;
    const auto& cat       = vodCategories[index];
    requestVODStreams(cat.category_id, cat.category_name);
    streamGrid->showSkeleton();
}

void HomeVOD::buildStreams(const std::vector<tsvitch::XtreamVODStream>& streams, const std::string& categoryName) {
    currentLiveList.clear();
    currentLiveList.reserve(streams.size());

    for (const auto& s : streams) {
        tsvitch::LiveM3u8 live;
        live.id          = s.stream_id;
        live.chno        = std::to_string(s.num);
        live.title       = s.name;
        live.logo        = s.stream_icon;
        live.groupTitle  = categoryName;
        std::string ext  = s.container_extension.empty() ? "mkv" : s.container_extension;
        live.url         = tsvitch::XtreamAPI::instance().getVODUrl(s.stream_id, ext);
        currentLiveList.push_back(live);
    }

    streamDataSource = std::make_unique<VodStreamDataSource>(currentLiveList);
    streamGrid->setDataSource(streamDataSource.get());
}

brls::View* HomeVOD::create() { return new HomeVOD(); }
