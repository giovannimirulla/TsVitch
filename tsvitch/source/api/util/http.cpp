

#include "tsvitch/util/http.hpp"
#include <borealis.hpp> // or the specific header where brls::Logger is defined

namespace tsvitch {

cpr::Response HTTP::get(const std::string& url, const cpr::Parameters& parameters, int timeout) {
    return cpr::Get(cpr::Url{url}, parameters, CPR_HTTP_BASE);
}

void HTTP::setProxy(const std::string& proxyUrl) {
    if (proxyUrl.empty()) {
        HTTP::PROXIES = {};
        brls::Logger::info("Proxy disabled");
    } else {
        HTTP::PROXIES = cpr::Proxies{{"http", proxyUrl}, {"https", proxyUrl}};
        brls::Logger::info("Proxy configured: {}", proxyUrl);
    }
}

};  // namespace tsvitch