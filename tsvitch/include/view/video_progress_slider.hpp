#pragma once

#include <vector>
#include <borealis/core/box.hpp>

namespace brls {
class Rectangle;
}
class SVGImage;

class VideoProgressSlider : public brls::Box
{
public:
    VideoProgressSlider();

    ~VideoProgressSlider() override;

    static brls::View* create();

    void onLayout() override;

    brls::View* getDefaultFocus() override;

    void onChildFocusLost(brls::View* directChild, brls::View* focusedView) override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style,
              brls::FrameContext* ctx) override;

    void setProgress(float progress);

    [[nodiscard]] float getProgress() const { return progress; }

    brls::Event<float>* getProgressEvent() { return &progressEvent; }

    brls::Event<float>* getProgressSetEvent() { return &progressSetEvent; }

    void addClipPoint(float point);

    void clearClipPoint();

    void setClipPoint(const std::vector<float>& data);

    void setDisabledPointerGesture(bool disabled);


    const std::vector<float>& getClipPoint();

protected:
    bool disabledPointerGesture = false;

private:
    brls::InputManager* input;
    brls::Rectangle* line;
    brls::Rectangle* lineEmpty;
    SVGImage* pointerIcon;
    brls::Box* pointer;

    brls::Event<float> progressEvent;
    brls::Event<float> progressSetEvent;

    std::vector<float> clipPointList;

    float progress       = 1;
    bool pointerSelected = false;

    bool ignoreProgressSetting = false;

    float lastProgress = 1;

    void buttonsProcessing();
    void updateUI();
    bool cancelPointerChange();
    void updateGestures();
    void clearGestureRecognizers();
};