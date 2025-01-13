#pragma once

#include <string>
#include <vector>
#include <pystring.h>
#include <nlohmann/json.hpp>
#include <borealis/core/singleton.hpp>

class ShaderProfile {
public:
    std::string name;
    std::vector<std::string> shaders;
    std::vector<std::vector<std::string>> settings;

    std::string getShaderString();
};

inline void to_json(nlohmann::json& nlohmann_json_j, const ShaderProfile& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, name, shaders, settings));
}

inline void from_json(const nlohmann::json& nlohmann_json_j, ShaderProfile& nlohmann_json_t) {
    if (nlohmann_json_j.contains("name")) {
        auto& name = nlohmann_json_j.at("name");
        if (name.is_string()) name.get_to(nlohmann_json_t.name);
    }
    if (nlohmann_json_j.contains("shaders")) {
        auto& shaders = nlohmann_json_j.at("shaders");
        if (shaders.is_array()) shaders.get_to(nlohmann_json_t.shaders);
    }
    if (nlohmann_json_j.contains("settings")) {
        auto& settings = nlohmann_json_j.at("settings");
        if (settings.is_array()) settings.get_to(nlohmann_json_t.settings);
        for (auto& setting : nlohmann_json_t.settings) {
            if (setting.empty()) continue;
            
            const std::vector<std::string> mainOptions = {"set", "run", "change-list"};
            bool hasMainOption                         = false;
            for (auto& option : mainOptions) {
                if (pystring::endswith(setting[0], option)) {
                    hasMainOption = true;
                    break;
                }
            }
            
            if (!hasMainOption) setting.insert(setting.begin(), "set");
        }
    }
}

typedef std::vector<ShaderProfile> ShaderProfileList;

class AnimeProfile {
public:
    AnimeProfile() = default;
    AnimeProfile(size_t anime, std::string profile) : anime(anime), profile(std::move(profile)) {}

    size_t anime = 0;
    std::string profile;
};

inline void to_json(nlohmann::json& nlohmann_json_j, const AnimeProfile& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, anime, profile));
}

inline void from_json(const nlohmann::json& nlohmann_json_j, AnimeProfile& nlohmann_json_t) {
    if (nlohmann_json_j.contains("anime")) {
        auto& anime = nlohmann_json_j.at("anime");
        if (anime.is_number()) anime.get_to(nlohmann_json_t.anime);
    }
    if (nlohmann_json_j.contains("profile")) {
        auto& profile = nlohmann_json_j.at("profile");
        if (profile.is_string()) profile.get_to(nlohmann_json_t.profile);
    }
}

typedef std::vector<AnimeProfile> AnimeProfileList;

class ShaderPack {
public:
    ShaderProfileList profiles;
    AnimeProfileList animeList;
};

inline void to_json(nlohmann::json& nlohmann_json_j, const ShaderPack& nlohmann_json_t) {
    NLOHMANN_JSON_EXPAND(NLOHMANN_JSON_PASTE(NLOHMANN_JSON_TO, profiles, animeList));
}

inline void from_json(const nlohmann::json& nlohmann_json_j, ShaderPack& nlohmann_json_t) {
    if (nlohmann_json_j.contains("profiles")) {
        auto& profiles = nlohmann_json_j.at("profiles");
        if (profiles.is_array()) profiles.get_to(nlohmann_json_t.profiles);
    }
    if (nlohmann_json_j.contains("animeList")) {
        auto& animeList = nlohmann_json_j.at("animeList");
        if (animeList.is_array()) animeList.get_to(nlohmann_json_t.animeList);
    }
}

class ShaderHelper : public brls::Singleton<ShaderHelper> {
public:
    ShaderHelper();

    void setShaderPack(const ShaderPack& data);

    ShaderPack getShaderPack();

    
    
    
    

    void setShader(size_t index, bool showHint = true);

    void clearShader(bool showHint = true);


    void setProfile(size_t sid, size_t profileIndex);

  
    void clearProfile(size_t sid);

 
    std::string getProfileBySeason(size_t sid);


    std::string getProfileByName(const std::string& name);


    std::string getProfileNameByIndex(size_t index);


    std::string getProfileShaderByIndex(size_t index);


    std::vector<std::vector<std::string>> getByProfileSettingByIndex(size_t index);


    size_t getProfileIndexBySeason(size_t sid);


    size_t getProfileIndexByName(const std::string& name);


    std::vector<std::string> getProfileList();

    [[nodiscard]] bool isAvailable() const;

    void load();

    void save();

private:
    ShaderPack pack;
};