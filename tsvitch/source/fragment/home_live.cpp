#include <utility>
#include <borealis/core/touch/tap_gesture.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/views/applet_frame.hpp>
#include <borealis/views/tab_frame.hpp>

#include "fragment/home_live.hpp"
#include "view/recycling_grid.hpp"
#include "view/video_card.hpp"
#include "view/grid_dropdown.hpp"
#include "utils/image_helper.hpp"
#include "utils/activity_helper.hpp"

using namespace brls::literals;

class DynamicUserInfoView : public RecyclingGridItem {
public:
    explicit DynamicUserInfoView(const std::string& xml) {
        this->inflateFromXMLRes(xml);
        auto theme = brls::Application::getTheme();
        selectedColor = theme.getColor("color/tsvitch");
        fontColor = theme.getColor("brls/text");
    }

    void setUserInfo(const std::string& avatar, const std::string& username, bool isUpdate = false) {
        this->labelUsername->setText(username);
    }



    void setSelected(bool selected) {
        this->labelUsername->setTextColor(selected ? selectedColor: fontColor);
    }


    void prepareForReuse() override {}

    void cacheForReuse() override {
 
    }

    static RecyclingGridItem* create(const std::string& xml = "xml/views/user_info_dynamic.xml") {
        return new DynamicUserInfoView(xml);
    }

private:
    BRLS_BIND(brls::Label, labelUsername, "username");
    NVGcolor selectedColor{};
    NVGcolor fontColor{};
};

class DataSourceUpList : public RecyclingGridDataSource {
public:
    explicit DataSourceUpList(std::vector<std::string>  result) : list(std::move(result)) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {

        DynamicUserInfoView* item = (DynamicUserInfoView*)recycler->dequeueReusableCell("Cell");

        auto& r = this->list[index];
        item->setUserInfo(r,r);

        return item;
    }

    size_t getItemCount() override { return list.size() ; }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        // 取消选中
        std::vector<RecyclingGridItem*>& items = recycler->getGridItems();
        for (auto& i : items) {
            auto* cell = dynamic_cast<DynamicUserInfoView*>(i);
            if (cell) cell->setSelected(false);
        }

        selectedIndex = index;

    }

    void appendData(const std::vector<std::string>& data) {
        this->list.insert(this->list.end(), data.begin(), data.end());
    }


    void clearData() override { this->list.clear(); }

private:
    std::vector<std::string>  list;
    size_t selectedIndex = 0;
};


const std::string GridSubAreaCellContentXML = R"xml(
<brls:Box
        width="auto"
        height="@style/brls/sidebar/item_height"
        focusable="true"
        paddingTop="12.5"
        paddingBottom="12.5"
        axis="column"
        alignItems="center">

    <brls:Image
        id="area/avatar"
        scalingType="fill"
        cornerRadius="4"
        width="50"
        height="50"/>

    <brls:Label
            id="area/title"
            width="auto"
            height="auto"
            grow="1"
            fontSize="14" />

</brls:Box>
)xml";

class GridSubAreaCell : public RecyclingGridItem {
public:
    GridSubAreaCell() { this->inflateFromXMLString(GridSubAreaCellContentXML); }

    void setData(const std::string& name, const std::string& pic) {
        title->setText(name);
        if (pic.empty()) {
            this->image->setImageFromRes("pictures/22_open.png");
        } else {
            ImageHelper::with(image)->load(pic + ImageHelper::face_ext);
        }
    }

    void prepareForReuse() override { this->image->setImageFromRes("pictures/video-card-bg.png"); }

    void cacheForReuse() override { ImageHelper::clear(this->image); }

    static RecyclingGridItem* create() { return new GridSubAreaCell(); }

protected:
    BRLS_BIND(brls::Label, title, "area/title");
    BRLS_BIND(brls::Image, image, "area/avatar");
};

const std::string GridMainAreaCellContentXML = R"xml(
<brls:Box
        width="auto"
        height="@style/brls/sidebar/item_height"
        focusable="true"
        paddingTop="12.5"
        paddingBottom="12.5"
        alignItems="center">

    <brls:Image
        id="area/avatar"
        scalingType="fill"
        cornerRadius="4"
        marginLeft="10"
        marginRight="10"
        width="40"
        height="40"/>

    <brls:Label
            id="area/title"
            width="auto"
            height="auto"
            grow="1"
            fontSize="22" />

</brls:Box>
)xml";

class GridMainAreaCell : public RecyclingGridItem {
public:
    GridMainAreaCell() { this->inflateFromXMLString(GridMainAreaCellContentXML); }

    void setData(const std::string& name, const std::string& pic) {
        title->setText(name);
        if (pic.empty()) {
            this->image->setImageFromRes("pictures/22_open.png");
        } else {
            ImageHelper::with(image)->load(pic + ImageHelper::face_ext);
        }
    }

    void setSelected(bool value) { this->title->setTextColor(value ? selectedColor : fontColor); }

    void prepareForReuse() override { this->image->setImageFromRes("pictures/video-card-bg.png"); }

    void cacheForReuse() override { ImageHelper::clear(this->image); }

    static RecyclingGridItem* create() { return new GridMainAreaCell(); }

protected:
    BRLS_BIND(brls::Label, title, "area/title");
    BRLS_BIND(brls::Image, image, "area/avatar");

    NVGcolor selectedColor = brls::Application::getTheme().getColor("color/tsvitch");
    NVGcolor fontColor     = brls::Application::getTheme().getColor("brls/text");
};

class DataSourceLiveVideoList : public RecyclingGridDataSource {
public:
    explicit DataSourceLiveVideoList(tsvitch::LiveM3u8ListResult result) : videoList(std::move(result)) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        RecyclingGridItemLiveVideoCard* item = (RecyclingGridItemLiveVideoCard*)recycler->dequeueReusableCell("Cell");

        tsvitch::LiveM3u8& r = this->videoList[index];
        item->setCard(r.logo, r.title, r.groupTitle, r.chno);
        return item;
    }

    size_t getItemCount() override { return videoList.size(); }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        Intent::openLive(videoList[index].url, videoList[index].title, videoList[index].groupTitle);
    }

    void appendData(const tsvitch::LiveM3u8ListResult& data) {
        this->videoList.insert(this->videoList.end(), data.begin(), data.end());
    }

    void clearData() override { this->videoList.clear(); }

private:
    tsvitch::LiveM3u8ListResult videoList;
};

HomeLive::HomeLive() {
    this->inflateFromXMLRes("xml/fragment/home_live.xml");
    brls::Logger::debug("Fragment HomeLive: create");
    recyclingGrid->registerCell("Cell", []() { return RecyclingGridItemLiveVideoCard::create(); });

    upRecyclingGrid->registerCell("Cell", []() { return DynamicUserInfoView::create(); });

    this->requestLiveList();
}

void HomeLive::onLiveList(const tsvitch::LiveM3u8ListResult& result) {
    brls::Threading::sync([this, result]() {
        auto* datasource = dynamic_cast<DataSourceLiveVideoList*>(recyclingGrid->getDataSource());
        if (datasource) {
            if (result.empty()) return;
            if (!result.empty()) {
                datasource->appendData(result);
                recyclingGrid->notifyDataChanged();
            }
        } else {
            if (result.empty())
                recyclingGrid->setEmpty();
            else
                recyclingGrid->setDataSource(new DataSourceLiveVideoList(result));
        }

        //set list inside upRecyclingGrid of unique groupTitle inside result
        std::map<std::string, std::vector<tsvitch::LiveM3u8>> groupMap;
        for (const auto& item : result) {
            groupMap[item.groupTitle].push_back(item);
        }
        std::vector<std::string> groupTitles;
        for (const auto& item : groupMap) {
            groupTitles.push_back(item.first);
        }
        upRecyclingGrid->setDataSource(new DataSourceUpList(groupTitles));
    });
}

void HomeLive::onCreate() {
   brls::Logger::debug("Fragment HomeLive: onCreate");


    // for (int i = 0; i < 100; ++i) {
    //     // Crea la sidebar item (puoi personalizzare label e stile)
    //    auto* item = new AutoSidebarItem();
    //         item->setTabStyle(AutoTabBarStyle::PLAIN);
    //         item->setLabel("Tab " + std::to_string(i + 1));
    //         item->setFontSize(18);

    //     // Funzione che crea la view associata al tab
    //        this->tabFrame->addTab(item, [this, i, item]() {
    //         // Qui puoi restituire una view diversa per ogni tab
    //         // Esempio: una semplice Box con un'etichetta
    //         auto* box = new brls::Box();
    //         auto* label = new brls::Label();
    //         label->setText("Contenuto Tab " + std::to_string(i + 1));
    //         box->addView(label);
    //         return box;
    //     });
    // }
}

void HomeLive::onError(const std::string& error) {
    brls::sync([this, error]() { this->recyclingGrid->setError(error); });
}

HomeLive::~HomeLive() { brls::Logger::debug("Fragment HomeLiveActivity: delete"); }

brls::View* HomeLive::create() { return new HomeLive(); }