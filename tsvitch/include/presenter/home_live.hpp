//
// Created by fang on 2022/7/13.
//

#pragma once

#include "tsvitch/result/home_live_result.h"
#include "presenter/presenter.h"

class HomeLiveRequest : public Presenter {
public:
    virtual void onLiveList(const tsvitch::LiveM3u8ListResult& result, bool firstLoad);

    virtual void onError(const std::string& error) = 0;


    void requestLiveList();

};