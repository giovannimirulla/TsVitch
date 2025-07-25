#include <utility>
#include "view/recycling_grid.hpp"
#include "view/button_refresh.hpp"

RecyclingGridItem::RecyclingGridItem() {
    this->setFocusable(true);
    this->registerClickAction([this](View* view) {
        auto* recycler = dynamic_cast<RecyclingGrid*>(getParent()->getParent());
        if (recycler) recycler->getDataSource()->onItemSelected(recycler, index);
        return true;
    });
    this->addGestureRecognizer(new brls::TapGestureRecognizer(this));
}

size_t RecyclingGridItem::getIndex() const { return this->index; }

void RecyclingGridItem::setIndex(size_t value) { this->index = value; }

RecyclingGridItem::~RecyclingGridItem() = default;

SkeletonCell::SkeletonCell() { this->setFocusable(false); }

RecyclingGridItem* SkeletonCell::create() { return new SkeletonCell(); }

void SkeletonCell::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                        brls::FrameContext* ctx) {
    brls::Time curTime = brls::getCPUTimeUsec() / 1000;
    float p            = (curTime % 1000) * 1.0 / 1000;
    p                  = fabs(0.5 - p) + 0.25;

    NVGcolor end = background;
    end.a        = p;

    NVGpaint paint = nvgLinearGradient(vg, x, y, x + width, y + height, a(background), a(end));
    nvgBeginPath(vg);
    nvgFillPaint(vg, paint);
    nvgRoundedRect(vg, x, y, width, height, 6);
    nvgFill(vg);
}

class DataSourceSkeleton : public RecyclingGridDataSource {
public:
    DataSourceSkeleton(unsigned int n) : num(n) {}
    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) {
        SkeletonCell* item = (SkeletonCell*)recycler->dequeueReusableCell("Skeleton");
        item->setHeight(recycler->estimatedRowHeight);
        return item;
    }

    size_t getItemCount() { return this->num; }

    void clearData() { this->num = 0; }

private:
    unsigned int num;
};

RecyclingGrid::RecyclingGrid() {
    brls::Logger::debug("View RecyclingGrid: create");

    this->hintImage = new brls::Image();
    this->hintImage->detach();
    this->hintImage->setImageFromRes("pictures/empty.png");
    this->hintLabel = new brls::Label();
    this->hintLabel->detach();
    this->hintLabel->setFontSize(14);
    this->hintLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);

    this->setFocusable(false);

    this->setScrollingBehavior(brls::ScrollingBehavior::CENTERED);

    this->contentBox = new RecyclingGridContentBox(this);
    this->setContentView(this->contentBox);

    this->registerFloatXMLAttribute("itemHeight", [this](float value) {
        this->estimatedRowHeight = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("spanCount", [this](float value) {
        if (value != 1) {
            isFlowMode = false;
        }
        this->spanCount = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("itemSpace", [this](float value) {
        this->estimatedRowSpace = value;
        this->reloadData();
    });

    this->registerFloatXMLAttribute("preFetchLine", [this](float value) {
        this->preFetchLine = value;
        this->reloadData();
    });

    this->registerBoolXMLAttribute("flowMode", [this](bool value) {
        this->spanCount  = 1;
        this->isFlowMode = value;
        this->reloadData();
    });

    this->registerPercentageXMLAttribute("paddingRight",
                                         [this](float percentage) { this->setPaddingRightPercentage(percentage); });

    this->registerPercentageXMLAttribute("paddingLeft",
                                         [this](float percentage) { this->setPaddingLeftPercentage(percentage); });

    this->registerCell("Skeleton", []() { return SkeletonCell::create(); });
    this->showSkeleton();
}

RecyclingGrid::~RecyclingGrid() {
    brls::Logger::debug("View RecyclingGridActivity: delete");
    if (this->hintImage) this->hintImage->freeView();
    this->hintImage = nullptr;
    if (this->hintLabel) this->hintLabel->freeView();
    this->hintLabel = nullptr;
    delete this->dataSource;
    for (const auto& it : queueMap) {
        for (auto item : *it.second) {
            item->setParent(nullptr);
            if (item->isPtrLocked())
                item->freeView();
            else
                delete item;
        }
        delete it.second;
    }
}

void RecyclingGrid::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
                         brls::FrameContext* ctx) {
    itemsRecyclingLoop();

    ScrollingFrame::draw(vg, x, y, width, height, style, ctx);

    if (!this->dataSource || this->dataSource->getItemCount() == 0) {
        if (!this->hintImage) return;
        float w1 = hintImage->getWidth(), w2 = hintLabel->getWidth();
        float h1 = hintImage->getHeight(), h2 = hintLabel->getHeight();
        this->hintImage->setAlpha(this->getAlpha());
        this->hintImage->draw(vg, x + (width - w1) / 2, y + (height - h1) / 2, w1, h1, style, ctx);
        this->hintLabel->setAlpha(this->getAlpha());
        this->hintLabel->draw(vg, x + (width - w2) / 2, y + (height + h1) / 2, w2, h2, style, ctx);
    }
}

void RecyclingGrid::registerCell(std::string identifier, std::function<RecyclingGridItem*()> allocation) {
    queueMap.insert(std::make_pair(identifier, new std::vector<RecyclingGridItem*>()));
    allocationMap.insert(std::make_pair(identifier, allocation));
}

void RecyclingGrid::addCellAt(size_t index, bool downSide) {
        brls::Logger::info("addCellAt: index={}", index);
            
    RecyclingGridItem* cell;

    cell = dataSource->cellForRow(this, index);

    float cellHeight = estimatedRowHeight;
    float cellWidth  = (renderedFrame.getWidth() - getPaddingLeft() - getPaddingRight()) / spanCount -
                      cell->getMarginLeft() - cell->getMarginRight();
    float cellX = renderedFrame.getMinX() + getPaddingLeft();

    if (isFlowMode) {
        cell->setWidth(cellWidth);
        if (cellHeightCache[index] == -1) {
            cellHeight = cell->getHeight();

            if (cellHeight > estimatedRowHeight) {
                cellHeight = estimatedRowHeight;
            }
            cellHeightCache[index] = cellHeight;
        } else {
            cellHeight = cellHeightCache[index];
        }

        brls::Logger::verbose("Add cell at: y {} height {}", getHeightByCellIndex(index) + paddingTop, cellHeight);
    } else {
        cell->setWidth(cellWidth - estimatedRowSpace);
        cellX += (renderedFrame.getWidth() - getPaddingLeft() - getPaddingRight()) / spanCount * (index % spanCount);
    }

    cell->setHeight(cellHeight);
    cell->setDetachedPositionX(cellX);
    cell->setDetachedPositionY(getHeightByCellIndex(index) + paddingTop);
    cell->setIndex(index);

    this->contentBox->getChildren().insert(this->contentBox->getChildren().end(), cell);

    size_t* userdata = (size_t*)malloc(sizeof(size_t));
    *userdata        = index;

    cell->setParent(this->contentBox, userdata);

    this->contentBox->invalidate();
    cell->View::willAppear();

    if (index < visibleMin) visibleMin = index;

    if (index > visibleMax) visibleMax = index;

    if (index % spanCount == 0) {
        if (!downSide) renderedFrame.origin.y -= cellHeight + estimatedRowSpace;

        renderedFrame.size.height += cellHeight + estimatedRowSpace;
    }

    if (isFlowMode)
        contentBox->setHeight(getHeightByCellIndex(this->dataSource->getItemCount()) + paddingTop + paddingBottom);

    brls::Logger::verbose("Cell #{} - added", index);
}

void RecyclingGrid::setDataSource(RecyclingGridDataSource* source) {
    if (this->dataSource) delete this->dataSource;

    this->requestNextPage = false;
    this->dataSource      = source;
    if (layouted) reloadData();
}

void RecyclingGrid::reloadData() {
    brls::Logger::debug("RecyclingGrid reloadData");
    if (!layouted) return;

    auto children = this->contentBox->getChildren();
    for (auto const& child : children) {
        queueReusableCell((RecyclingGridItem*)child);
        this->contentBox->removeView(child, false);
    }

    visibleMin = UINT_MAX;
    visibleMax = 0;

    renderedFrame            = brls::Rect();
    renderedFrame.size.width = getWidth();
    if (renderedFrame.size.width != renderedFrame.size.width) {
        renderedFrame.size.width = oldWidth;
    }

    setContentOffsetY(0, false);
    if (dataSource == nullptr) return;
    if (dataSource->getItemCount() <= 0) {
        contentBox->setHeight(0);
        return;
    }
    size_t cellFocusIndex = this->defaultCellFocus;
    if (cellFocusIndex >= dataSource->getItemCount()) cellFocusIndex = dataSource->getItemCount() - 1;

    if (!isFlowMode || spanCount != 1) {
        contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) * (float)getRowCount() + paddingTop +
                              paddingBottom);

        size_t lineHeadIndex = cellFocusIndex / spanCount * spanCount;

        renderedFrame.origin.y = getHeightByCellIndex(lineHeadIndex);

        this->addCellAt(lineHeadIndex, true);
    } else {
        cellHeightCache.clear();
        for (size_t section = 0; section < dataSource->getItemCount(); section++) {
            float height = dataSource->heightForRow(this, section);
            cellHeightCache.push_back(height);
        }
        contentBox->setHeight(getHeightByCellIndex(dataSource->getItemCount()) + paddingTop + paddingBottom);

        this->addCellAt(0, true);
    }

    selectRowAt(cellFocusIndex, false);
}

void RecyclingGrid::notifyDataChanged() {
    brls::Logger::debug("RecyclingGrid notifyDataChanged");
    if (!layouted) return;

    if (dataSource) {
        if (isFlowMode) {
            for (size_t i = cellHeightCache.size(); i < dataSource->getItemCount(); i++) {
                float height = dataSource->heightForRow(this, i);
                cellHeightCache.push_back(height);
            }
            contentBox->setHeight(getHeightByCellIndex(this->dataSource->getItemCount()) + paddingTop + paddingBottom);
        } else {
            contentBox->setHeight((estimatedRowHeight + estimatedRowSpace) * this->getRowCount() + paddingTop +
                                  paddingBottom);
        }
    }

    requestNextPage = false;
}

RecyclingGridItem* RecyclingGrid::getGridItemByIndex(size_t index) {
    for (brls::View* i : contentBox->getChildren()) {
        RecyclingGridItem* v = dynamic_cast<RecyclingGridItem*>(i);
        if (!v) continue;
        if (v->getIndex() == index) return v;
    }

    return nullptr;
}

std::vector<RecyclingGridItem*>& RecyclingGrid::getGridItems() {
    return (std::vector<RecyclingGridItem*>&)contentBox->getChildren();
}

void RecyclingGrid::clearData() {
    if (dataSource) {
        dataSource->clearData();
        this->reloadData();
    }
}

void RecyclingGrid::setEmpty(std::string msg) {
    this->hintImage->setImageFromRes("pictures/empty.png");
    this->hintLabel->setText(msg);
    this->clearData();
}

void RecyclingGrid::setError(std::string error) {
    this->hintImage->setImageFromRes("pictures/net_error.png");
    this->hintLabel->setText(error);
    this->clearData();
}

void RecyclingGrid::setDefaultCellFocus(size_t index) { this->defaultCellFocus = index; }

size_t RecyclingGrid::getDefaultCellFocus() const { return this->defaultCellFocus; }

size_t RecyclingGrid::getItemCount() { return this->dataSource->getItemCount(); }

size_t RecyclingGrid::getRowCount() { return (this->dataSource->getItemCount() - 1) / this->spanCount + 1; }

void RecyclingGrid::onNextPage(const std::function<void()>& callback) { this->nextPageCallback = callback; }

void RecyclingGrid::itemsRecyclingLoop() {
    if (!dataSource) return;

    brls::Rect visibleFrame = getVisibleFrame();

    // while (true) {
    //     RecyclingGridItem* minCell = nullptr;
    //     for (auto it : contentBox->getChildren())

    //         if (*((size_t*)it->getParentUserData()) == visibleMin) minCell = (RecyclingGridItem*)it;

    //     if (!minCell || (minCell->getDetachedPosition().y +
    //                          getHeightByCellIndex(visibleMin + (preFetchLine + 1) * spanCount, visibleMin) >=
    //                      visibleFrame.getMinY()))
    //         break;

    //     float cellHeight = estimatedRowHeight;
    //     if (isFlowMode) cellHeight = cellHeightCache[visibleMin];

    //     renderedFrame.origin.y += minCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;
    //     renderedFrame.size.height -= minCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;

    //     queueReusableCell(minCell);
    //     this->contentBox->removeView(minCell, false);

    //     brls::Logger::verbose("Cell #{} - destroyed", visibleMin);

    //     visibleMin++;
    // }

    // while (true) {
    //     RecyclingGridItem* maxCell = nullptr;

    //     for (auto it : contentBox->getChildren())
    //         if (*((size_t*)it->getParentUserData()) == visibleMax) maxCell = (RecyclingGridItem*)it;

    //     if (!maxCell || (maxCell->getDetachedPosition().y -
    //                          getHeightByCellIndex(visibleMax, visibleMax - preFetchLine * spanCount) <=
    //                      visibleFrame.getMaxY()))
    //         break;
    //     if (visibleMax == 0) {
    //         break;
    //     }

    //     float cellHeight = estimatedRowHeight;
    //     if (isFlowMode) cellHeight = cellHeightCache[visibleMax];

    //     renderedFrame.size.height -= maxCell->getIndex() % spanCount == 0 ? cellHeight + estimatedRowSpace : 0;

    //     queueReusableCell(maxCell);
    //     this->contentBox->removeView(maxCell, false);

    //     brls::Logger::verbose("Cell #{} - destroyed", visibleMax);

    //     visibleMax--;
    // }

    while (visibleMin - 1 < dataSource->getItemCount()) {
        if ((visibleMin) % spanCount == 0)

            if (renderedFrame.getMinY() + getHeightByCellIndex(visibleMin + preFetchLine * spanCount, visibleMin) <
                visibleFrame.getMinY() - paddingTop) {
                break;
            }
        addCellAt(visibleMin - 1, false);
    }

    while (visibleMax + 1 < dataSource->getItemCount()) {
        if ((visibleMax + 1) % spanCount == 0)

            if (renderedFrame.getMaxY() -
                    getHeightByCellIndex(visibleMax + 1, visibleMax + 1 - preFetchLine * spanCount) >
                visibleFrame.getMaxY() - paddingBottom) {
                requestNextPage = false;
                break;
            }
        addCellAt(visibleMax + 1, true);
    }

    if (visibleMax + 1 >= this->getItemCount()) {
        if (!requestNextPage && nextPageCallback) {
            if (dataSource && !dynamic_cast<DataSourceSkeleton*>(dataSource) && dataSource->getItemCount() > 0) {
                brls::Logger::debug("RecyclingGrid request next page");
                requestNextPage = true;
                this->nextPageCallback();
            }
        }
    }
}

RecyclingGridDataSource* RecyclingGrid::getDataSource() const { return this->dataSource; }

void RecyclingGrid::showSkeleton(unsigned int num) { this->setDataSource(new DataSourceSkeleton(num)); }

void RecyclingGrid::selectRowAt(size_t index, bool animated) {
    this->setContentOffsetY(getHeightByCellIndex(index), animated);
    this->itemsRecyclingLoop();

    for (View* view : contentBox->getChildren()) {
        if (*((size_t*)view->getParentUserData()) == index) {
            contentBox->setLastFocusedView(view);
            break;
        }
    }
}

float RecyclingGrid::getHeightByCellIndex(size_t index, size_t start) {
    if (index <= start) return 0;
    if (!isFlowMode) return (estimatedRowHeight + estimatedRowSpace) * (size_t)((index - start) / spanCount);

    if (cellHeightCache.size() == 0) {
        brls::Logger::error("cellHeightCache.size() cannot be zero in flow mode {} {}", start, index);
        return 0;
    }

    if (start < 0) start = 0;
    if (index > this->cellHeightCache.size()) index = this->cellHeightCache.size();

    float res = 0;
    for (size_t i = start; i < index && i < cellHeightCache.size(); i++) {
        if (cellHeightCache[i] != -1)
            res += cellHeightCache[i] + estimatedRowSpace;
        else
            res += estimatedRowHeight + estimatedRowSpace;
    }
    return res;
}

void RecyclingGrid::forceRequestNextPage() { this->requestNextPage = false; }

brls::View* RecyclingGrid::getNextCellFocus(brls::FocusDirection direction, brls::View* currentView) {
    void* parentUserData = currentView->getParentUserData();

    if ((this->contentBox->getAxis() == brls::Axis::ROW && direction != brls::FocusDirection::LEFT &&
         direction != brls::FocusDirection::RIGHT)) {
        int row_offset = spanCount;
        if (direction == brls::FocusDirection::UP) row_offset = -spanCount;
        View* row_currentFocus       = nullptr;
        size_t row_currentFocusIndex = *((size_t*)parentUserData) + row_offset;

        if (row_currentFocusIndex >= this->dataSource->getItemCount()) {
            row_currentFocusIndex -= *((size_t*)parentUserData) % spanCount;
        }

        while (!row_currentFocus && row_currentFocusIndex >= 0 &&
               row_currentFocusIndex < this->dataSource->getItemCount()) {
            for (auto it : this->contentBox->getChildren()) {
                if (*((size_t*)it->getParentUserData()) == row_currentFocusIndex) {
                    row_currentFocus = it->getDefaultFocus();
                    break;
                }
            }
            row_currentFocusIndex += row_offset;
        }
        if (row_currentFocus) {
            itemsRecyclingLoop();

            return row_currentFocus;
        }
    }

    if (this->contentBox->getAxis() == brls::Axis::ROW) {
        int position = *((size_t*)parentUserData) % spanCount;
        if ((direction == brls::FocusDirection::LEFT && position == 0) ||
            (direction == brls::FocusDirection::RIGHT && position == (spanCount - 1))) {
            View* next = getParentNavigationDecision(this, nullptr, direction);
            if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
            return next;
        }
    }

    if ((this->contentBox->getAxis() == brls::Axis::ROW && direction != brls::FocusDirection::LEFT &&
         direction != brls::FocusDirection::RIGHT) ||
        (this->contentBox->getAxis() == brls::Axis::COLUMN && direction != brls::FocusDirection::UP &&
         direction != brls::FocusDirection::DOWN)) {
        View* next = getParentNavigationDecision(this, nullptr, direction);
        if (!next && hasParent()) next = getParent()->getNextFocus(direction, this);
        return next;
    }

    size_t offset = 1;

    if ((this->contentBox->getAxis() == brls::Axis::ROW && direction == brls::FocusDirection::LEFT) ||
        (this->contentBox->getAxis() == brls::Axis::COLUMN && direction == brls::FocusDirection::UP)) {
        offset = -1;
    }

    size_t currentFocusIndex = *((size_t*)parentUserData) + offset;
    View* currentFocus       = nullptr;

    while (!currentFocus && currentFocusIndex >= 0 && currentFocusIndex < this->dataSource->getItemCount()) {
        for (auto it : this->contentBox->getChildren()) {
            if (*((size_t*)it->getParentUserData()) == currentFocusIndex) {
                currentFocus = it->getDefaultFocus();
                break;
            }
        }
        currentFocusIndex += offset;
    }

    currentFocus = getParentNavigationDecision(this, currentFocus, direction);
    if (!currentFocus && hasParent()) currentFocus = getParent()->getNextFocus(direction, this);
    return currentFocus;
}

void RecyclingGrid::onLayout() {
    ScrollingFrame::onLayout();
    auto rect   = this->getFrame();
    float width = rect.getWidth();

    if (width != width) return;

    if (!this->contentBox) return;
    this->contentBox->setWidth(width);
    if (checkWidth()) {
        brls::Logger::debug("RecyclingGrid::onLayout reloadData()");
        layouted = true;
        reloadData();
    }
}

bool RecyclingGrid::checkWidth() {
    float width = getWidth();
    if (oldWidth == -1) {
        oldWidth = width;
    }
    if ((int)oldWidth != (int)width && width != 0) {
        brls::Logger::debug("RecyclingGrid::checkWidth from {} to {}", oldWidth, width);
        oldWidth = width;
        return true;
    }
    oldWidth = width;
    return false;
}

void RecyclingGrid::queueReusableCell(RecyclingGridItem* cell) {
    brls::Logger::info("queueReusableCell: index={}", cell->getIndex());
    queueMap.at(cell->reuseIdentifier)->push_back(cell);
    cell->cacheForReuse();
}

void RecyclingGrid::setPadding(float padding) { this->setPadding(padding, padding, padding, padding); }

void RecyclingGrid::setPadding(float top, float right, float bottom, float left) {
    paddingPercentage = false;
    paddingTop        = top;
    paddingRight      = right;
    paddingBottom     = bottom;
    paddingLeft       = left;

    this->reloadData();
}

void RecyclingGrid::setPaddingTop(float top) {
    paddingTop = top;
    this->reloadData();
}

void RecyclingGrid::setPaddingRight(float right) {
    paddingPercentage = false;
    paddingRight      = right;
    this->reloadData();
}

void RecyclingGrid::setPaddingBottom(float bottom) {
    paddingBottom = bottom;
    this->reloadData();
}

void RecyclingGrid::setPaddingLeft(float left) {
    paddingPercentage = false;
    paddingLeft       = left;
    this->reloadData();
}

void RecyclingGrid::setPaddingRightPercentage(float right) {
    paddingPercentage = true;
    paddingRight      = right / 100.0f;
}

void RecyclingGrid::setPaddingLeftPercentage(float left) {
    paddingPercentage = true;
    paddingLeft       = left / 100.0f;
}

float RecyclingGrid::getPaddingLeft() {
    return paddingPercentage ? renderedFrame.getWidth() * paddingLeft : paddingLeft;
}

float RecyclingGrid::getPaddingRight() {
    return paddingPercentage ? renderedFrame.getWidth() * paddingRight : paddingRight;
}

brls::View* RecyclingGrid::getDefaultFocus() {
    if (this->dataSource && this->dataSource->getItemCount() > 0) return ScrollingFrame::getDefaultFocus();
    return nullptr;
}

brls::View* RecyclingGrid::create() { return new RecyclingGrid(); }

RecyclingGridItem* RecyclingGrid::dequeueReusableCell(std::string identifier) {
    brls::Logger::verbose("RecyclingGrid::dequeueReusableCell: {}", identifier);
    RecyclingGridItem* cell = nullptr;
    auto it                 = queueMap.find(identifier);

    if (it != queueMap.end()) {
        std::vector<RecyclingGridItem*>* vector = it->second;
        if (!vector->empty()) {
            cell = vector->back();
            vector->pop_back();
        } else {
            cell                  = allocationMap.at(identifier)();
            cell->reuseIdentifier = identifier;
            cell->detach();
        }
    }

    cell->setHeight(brls::View::AUTO);
    if (cell) cell->prepareForReuse();

    return cell;
}

RecyclingGridContentBox::RecyclingGridContentBox(RecyclingGrid* recycler) : Box(brls::Axis::ROW), recycler(recycler) {}

brls::View* RecyclingGridContentBox::getNextFocus(brls::FocusDirection direction, brls::View* currentView) {
    return this->recycler->getNextCellFocus(direction, currentView);
}

RecyclingGridItem* RecyclingGrid::getFocusedItem() {
    brls::View* focused = brls::Application::getCurrentFocus();
    if (!focused)
        return nullptr;
    for (auto* item : this->getGridItems()) {
        // Cast a brls::View* per usare una lambda che controlla l'antenato
        if (item == focused || ([](brls::View* ancestor, brls::View* child) {
                while (child) {
                    if (child == ancestor)
                        return true;
                    child = child->getParent();
                }
                return false;
            })(static_cast<brls::View*>(item), focused))
            return item;
    }
    return nullptr;
}

