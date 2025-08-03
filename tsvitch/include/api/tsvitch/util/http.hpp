//
// Created by fang on 2022/5/1.
//

#pragma once

#include <nlohmann/json.hpp>
#include <cpr/cpr.h>

#include "tsvitch/util/md5.hpp"
#include "utils/number_helper.hpp"

#include "config/server_config.h"

#include <pystring.h>

namespace tsvitch {

using ErrorCallback = std::function<void(const std::string&, int code)>;
#define ERROR_MSG(msg, ...) \
    if (error) error(msg, __VA_ARGS__)
#ifdef CALLBACK
#undef CALLBACK
#endif
#define CALLBACK(data) \
    if (callback) callback(data)
#define CPR_HTTP_BASE                                                                               \
    cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS}, cpr::Timeout{tsvitch::HTTP::TIMEOUT}, \
        tsvitch::HTTP::HEADERS, tsvitch::HTTP::COOKIES, tsvitch::HTTP::PROXIES, tsvitch::HTTP::VERIFY

class HTTP {
public:
    static inline cpr::Cookies COOKIES = {false};
    static inline cpr::Header HEADERS  = {
        {"User-Agent", "tsvitch"},
    };
    static inline int TIMEOUT = 10000;
    static inline cpr::Proxies PROXIES = {};
    static inline cpr::VerifySsl VERIFY = true;

    static cpr::Response get(const std::string& url, const cpr::Parameters& parameters = {}, int timeout = 10000);

    static void setProxy(const std::string& proxyUrl);

    static void __cpr_post(const std::string& url, const cpr::Parameters& parameters = {},
                       const cpr::Body& body = cpr::Body{""},
                       const std::function<void(const cpr::Response&)>& callback = nullptr,
                       const ErrorCallback& error = nullptr) {
        auto headers_with_content_type = HTTP::HEADERS;
        headers_with_content_type["Content-Type"] = "application/json";
        headers_with_content_type["Authorization"] = std::string("Bearer ") + SERVER_TOKEN_VALUE;
        cpr::PostCallback(
            [callback, error](const cpr::Response& r) {
                if (r.error) {
                    ERROR_MSG(r.error.message, -1);
                    return;
                } else if (r.status_code != 200) {
                    ERROR_MSG("Network error. [Status code: " + std::to_string(r.status_code) + " ] - " + r.text, r.status_code);
                    return;
                }
                callback(r);
            },
            cpr::Url{url},
            parameters,
            body,
            cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
            cpr::Timeout{HTTP::TIMEOUT},
            headers_with_content_type,
            HTTP::COOKIES,
            HTTP::PROXIES,
            HTTP::VERIFY);
    }

    static void __cpr_get(const std::string& url, const cpr::Parameters& parameters = {},
                          int timeout = HTTP::TIMEOUT,
                          const std::function<void(const cpr::Response&)>& callback = nullptr,
                          const ErrorCallback& error = nullptr) {
        cpr::GetCallback(
            [callback, error](const cpr::Response& r) {
                if (r.error) {
                    ERROR_MSG(r.error.message, -1);
                    return;
                } else if (r.status_code != 200) {
                    ERROR_MSG("Network error. [Status code: " + std::to_string(r.status_code) + " ]", r.status_code);
                    return;
                }
                callback(r);
            },
            cpr::Url{url}, parameters,
            cpr::HttpVersion{cpr::HttpVersionCode::VERSION_2_0_TLS},
            cpr::Timeout{timeout},
            HTTP::HEADERS,
            HTTP::COOKIES,
            HTTP::PROXIES,
            HTTP::VERIFY);
    }

    template <typename ReturnType>
    static int parseJson(const cpr::Response& r, const std::function<void(ReturnType)>& callback = nullptr,
                          const ErrorCallback& error = nullptr) {
        try {
            nlohmann::json res = nlohmann::json::parse(r.text);
            int code           = res.at("code").get<int>();
            if (code == 0) {
                if (res.contains("data") && (res.at("data").is_object() || res.at("data").is_array())) {
                    CALLBACK(res.at("data").get<ReturnType>());
                    return 0;
                } else if (res.contains("result") && res.at("result").is_object()) {
                    CALLBACK(res.at("result").get<ReturnType>());
                    return 0;
                } else {
                    printf("data: %s\n", r.text.c_str());
                    ERROR_MSG("Cannot find data", -1);
                }
            } else if (res.at("message").is_string()) {
                ERROR_MSG(res.at("message").get<std::string>(), code);
            } else {
                ERROR_MSG("Param error", -1);
            }
        } catch (const std::exception& e) {
            ERROR_MSG("Api error. \n" + std::string{e.what()}, 200);
            printf("data: %s\n", r.text.c_str());
            printf("ERROR: %s\n", e.what());
        }
        return 1;
    }

};

}  // namespace tsvitch