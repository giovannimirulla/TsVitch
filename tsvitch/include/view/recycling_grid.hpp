#pragma once

#include <map>
#include <borealis/core/application.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/views/scrolling_frame.hpp>

namespace brls {
class Label;
class Image;
}  // namespace brls
class RecyclingGrid;

class RecyclingGridItem : public brls::Box {
public:
    RecyclingGridItem();
    ~RecyclingGridItem() override;

    size_t getIndex() const;

    void setIndex(size_t value);

    std::string reuseIdentifier;

    virtual void prepareForReuse() {}

    virtual void cacheForReuse() {}

private:
    size_t index;
};

class RecyclingGridDataSource {
public:
    virtual ~RecyclingGridDataSource() = default;

    virtual size_t getItemCount() { return 0; }

    virtual RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) { return nullptr; }

    virtual float heightForRow(RecyclingGrid* recycler, size_t index) { return -1; }

    virtual void onItemSelected(RecyclingGrid* recycler, size_t index) {}

    virtual void clearData() = 0;
};

class RecyclingGridContentBox;

class RecyclingGrid : public brls::ScrollingFrame {
public:
    RecyclingGrid();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation);

    void setDefaultCellFocus(size_t index);

    size_t getDefaultCellFocus() const;

    void setDataSource(RecyclingGridDataSource* source);

    RecyclingGridDataSource* getDataSource() const;

    void showSkeleton(unsigned int num = 30);

    void reloadData();

    void notifyDataChanged();

    RecyclingGridItem* getGridItemByIndex(size_t index);

    std::vector<RecyclingGridItem*>& getGridItems();

    void clearData();

    void setEmpty(std::string msg = "");

    void setError(std::string error = "");

    void selectRowAt(size_t index, bool animated);

    float getHeightByCellIndex(size_t index, size_t start = 0);

    View* getNextCellFocus(brls::FocusDirection direction, View* currentView);

    void forceRequestNextPage();

    void onLayout() override;

    size_t getItemCount();

    size_t getRowCount();

    void onNextPage(const std::function<void()>& callback = nullptr);

    void setPadding(float padding) override;
    void setPadding(float top, float right, float bottom, float left) override;
    void setPaddingTop(float top) override;
    void setPaddingRight(float right) override;
    void setPaddingBottom(float bottom) override;
    void setPaddingLeft(float left) override;

    void setPaddingRightPercentage(float right);
    void setPaddingLeftPercentage(float left);

    float getPaddingLeft();
    float getPaddingRight();

    RecyclingGridItem* dequeueReusableCell(std::string identifier);

    brls::View* getDefaultFocus() override;

    ~RecyclingGrid() override;

    static View* create();

    float estimatedRowSpace = 20;

    float estimatedRowHeight = 240;

    int spanCount = 4;

    int preFetchLine = 1;

    bool isFlowMode = false;

    RecyclingGridItem* getFocusedItem();

private:
    RecyclingGridDataSource* dataSource = nullptr;
    bool layouted                       = false;
    float oldWidth                      = -1;

    bool requestNextPage = false;

    uint32_t visibleMin, visibleMax;
    size_t defaultCellFocus = 0;

    float paddingTop    = 0;
    float paddingRight  = 0;
    float paddingBottom = 0;
    float paddingLeft   = 0;

    bool paddingPercentage = false;

    std::function<void()> nextPageCallback = nullptr;
    std::function<void()> refreshAction    = nullptr;

    RecyclingGridContentBox* contentBox = nullptr;
    brls::Image* hintImage;
    brls::Label* hintLabel;

    brls::Rect renderedFrame;
    std::vector<float> cellHeightCache;
    std::map<std::string, std::vector<RecyclingGridItem*>*> queueMap;
    std::map<std::string, std::function<RecyclingGridItem*(void)>> allocationMap;

    bool checkWidth();

    void queueReusableCell(RecyclingGridItem* cell);

    void itemsRecyclingLoop();

    void addCellAt(size_t index, bool downSide);
};

class RecyclingGridContentBox : public brls::Box {
public:
    RecyclingGridContentBox(RecyclingGrid* recycler);
    brls::View* getNextFocus(brls::FocusDirection direction, brls::View* currentView) override;

private:
    RecyclingGrid* recycler;
};

class SkeletonCell : public RecyclingGridItem {
public:
    SkeletonCell();

    static RecyclingGridItem* create();

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

private:
    NVGcolor background = brls::Application::getTheme()["color/grey_3"];
};