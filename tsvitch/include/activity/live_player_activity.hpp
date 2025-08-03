#pragma once

#include <functional>

#include <borealis/core/activity.hpp>
#include <borealis/core/bind.hpp>
#include <borealis/views/slider.hpp>

#include "utils/event_helper.hpp"
#include "presenter/live_data.hpp"

class VideoView;

class LiveActivity : public brls::Activity, public LiveDataRequest {
public:
    CONTENT_FROM_XML_RES("activity/video_activity.xml");

    explicit LiveActivity(const std::vector<tsvitch::LiveM3u8>& channels, size_t startIndex,
                          std::function<void()> onClose = nullptr);

    void setCommonData();

    void onContentAvailable() override;

    void onLiveData(std::string url) override;

    void onError(const std::string& error) override;

    std::vector<std::string> getQualityDescriptionList();
    int getCurrentQualityIndex();

    void retryRequestData();

    void startLive();

    void startAd(std::string adUrl);

    void detectContentType();

    void startDownload();

    std::string formatFileSize(size_t bytes);

    ~LiveActivity() override;

protected:
    VideoView* video = nullptr;

    std::function<void()> onCloseCallback;

    std::vector<tsvitch::LiveM3u8> channelList;
    size_t currentChannelIndex = 0;

    size_t toggleDelayIter = 0;

    size_t errorDelayIter = 0;

    bool isAd = false;

    tsvitch::LiveM3u8 liveData;

    MPVEvent::Subscription tl_event_id;
    bool mpvEventRegistered = false;

    CustomEvent::Subscription event_id;
    bool customEventRegistered = false;

    // Download state
    std::string currentDownloadId;
    bool hasActiveDownload = false;

private:
   void getAdUrlFromServer(std::function<void(const std::string&)> callback = nullptr);
};