#pragma once

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>

#include "utils/event_helper.hpp"
#include "presenter/live_data.hpp"

class VideoView;

class LiveActivity : public brls::Activity, public LiveDataRequest {
public:
    CONTENT_FROM_XML_RES("activity/video_activity.xml");

    explicit LiveActivity(const std::string& url = "", const std::string& name = "", const std::string& views = "");

    void setCommonData();

    void onContentAvailable() override;

    void onLiveData(std::string url) override;

    void onError(const std::string& error) override;

    std::vector<std::string> getQualityDescriptionList();
    int getCurrentQualityIndex();

    void retryRequestData();

    void startLive();

    void startAd(std::string adUrl);

    ~LiveActivity() override;

protected:
    BRLS_BIND(VideoView, video, "video");

    size_t toggleDelayIter = 0;

    size_t errorDelayIter = 0;

    tsvitch::LiveM3u8 liveData;

    MPVEvent::Subscription tl_event_id;

    CustomEvent::Subscription event_id;

private:
    std::string getAdUrlFromServer();
};