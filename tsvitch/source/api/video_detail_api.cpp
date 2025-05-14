#include <nlohmann/json.hpp>
#include <sstream>
#include <utility>
#include <vector>

#include "tsvitch.h"
#include "tsvitch/util/http.hpp"

#include "tsvitch/result/home_live_result.h"
#include "utils/config_helper.hpp"

namespace tsvitch {

/// Découpe une chaîne `a,b,c` -> {"a","b","c"}
static std::vector<std::string> split_csv(const std::string& csv)
{
    std::vector<std::string> out;
    std::stringstream        ss(csv);
    std::string              item;
    while (std::getline(ss, item, ',')) {
        if (!item.empty()) out.push_back(item);
    }
    return out;
}

nlohmann::json parse_m3u8_to_json(const std::string& m3u8_content)
{
    std::istringstream stream(m3u8_content);
    std::string        line;
    nlohmann::json     json_result = nlohmann::json::array();
    nlohmann::json     current_entry;

    while (std::getline(stream, line)) {
        if (line.rfind("#EXTINF:", 0) == 0) {
            if (!current_entry.empty()) {
                json_result.push_back(current_entry);
                current_entry.clear();
            }

            std::string extinf = line.substr(8);
            size_t      id_pos = extinf.find("tvg-id=\"");
            size_t      chno_pos = extinf.find("tvg-chno=\"");
            size_t      logo_pos = extinf.find("tvg-logo=\"");
            size_t      group_title_pos = extinf.find("group-title=\"");
            size_t      comma_pos = extinf.find_last_of(',');

            if (id_pos != std::string::npos) {
                size_t end_pos     = extinf.find('"', id_pos + 8);
                std::string id_val = extinf.substr(id_pos + 8, end_pos - (id_pos + 8));
                current_entry["id"]   = id_val;
                current_entry["tags"] = split_csv(id_val);     // <- NOUVEAU
            }
            if (chno_pos != std::string::npos) {
                size_t end_pos        = extinf.find('"', chno_pos + 10);
                current_entry["chno"] = extinf.substr(chno_pos + 10, end_pos - (chno_pos + 10));
            }
            if (logo_pos != std::string::npos) {
                size_t end_pos        = extinf.find('"', logo_pos + 10);
                current_entry["logo"] = extinf.substr(logo_pos + 10, end_pos - (logo_pos + 10));
            }
            if (group_title_pos != std::string::npos) {
                size_t end_pos              = extinf.find('"', group_title_pos + 13);
                current_entry["groupTitle"] = extinf.substr(group_title_pos + 13, end_pos - (group_title_pos + 13));
            }
            if (comma_pos != std::string::npos) {
                current_entry["title"] = extinf.substr(comma_pos + 1);
            }
        } else if (!line.empty() && line.rfind('#', 0) != 0) {
            current_entry["url"] = line;
        }
    }

    if (!current_entry.empty() && current_entry.contains("id"))
        json_result.push_back(current_entry);

#ifdef DEBUG
    std::cout << "json_result:\n" << json_result.dump(2) << std::endl;
#endif
    return json_result;
}

void TsVitchClient::get_file_m3u8(const std::function<void(LiveM3u8ListResult)>& callback,
                                  const ErrorCallback&                           error)
{
    auto m3u8Url = ProgramConfig::instance().getM3U8Url();
    HTTP::__cpr_get(
        m3u8Url,
        {},
        [callback, error](const cpr::Response& r) {
            try {
                nlohmann::json json_result = parse_m3u8_to_json(r.text);

                LiveM3u8ListResult result;
                for (const auto& item : json_result) {
                    LiveM3u8 live;
                    live.id         = item.value("id", "");
                    live.chno       = item.value("chno", "");
                    live.logo       = item.value("logo", "");
                    live.groupTitle = item.value("groupTitle", "");
                    live.title      = item.value("title", "");
                    live.url        = item.value("url", "");
                    result.push_back(live);
                }
                CALLBACK(result);
            } catch (const std::exception&) {
                ERROR_MSG("cannot get file m3u8", -1);
            }
        },
        error);
}

} // namespace tsvitch
