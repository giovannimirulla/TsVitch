#pragma once

#include <string>
#include <map>
#include <vector>
#include <future>

namespace tsvitch {

class LiveM3u8;
typedef std::vector<LiveM3u8> LiveM3u8ListResult;

using ErrorCallback = std::function<void(const std::string&, int code)>;

#define CLIENT tsvitch::TsVitchClient
#define CLIENT_ERR const std::string &error, int code

class TsVitchClient {
public:
    static void get_file_m3u8(const std::function<void(LiveM3u8ListResult)>& callback = nullptr,
                              const ErrorCallback& error                              = nullptr);

    static void get_xtream_channels(const std::function<void(LiveM3u8ListResult)>& callback = nullptr,
                                   const ErrorCallback& error                               = nullptr);

    static void get_xtream_channels_with_retry(const std::function<void(LiveM3u8ListResult)>& callback = nullptr,
                                              const ErrorCallback& error                               = nullptr,
                                              int maxRetries                                           = 3);

    static void get_live_channels(const std::function<void(LiveM3u8ListResult)>& callback = nullptr,
                                 const ErrorCallback& error                               = nullptr);

    static void register_user(
                              const std::function<void(const std::string&, int)>& callback = nullptr,
                              const ErrorCallback& error                                   = nullptr);

    static void check_user_id(
                              const std::function<void(const std::string&, int)>& callback = nullptr,
                              const ErrorCallback& error                                   = nullptr);

    static void get_ad(
                       const std::function<void(const std::string&, int)>& callback = nullptr,
                       const ErrorCallback& error                                   = nullptr);
};
}  // namespace tsvitch