#pragma once

#include <string>
#include <fstream>
#include <chrono>
#include <nlohmann/json.hpp>
#include <borealis/core/logger.hpp>
#include "utils/config_helper.hpp"

namespace tsvitch {

/**
 * Gestisce il salvataggio e ripristino delle posizioni di riproduzione dei video
 * Utilizza l'URL del video come chiave univoca
 */
class PlaybackPositionManager {
public:
    /**
     * Salva la posizione di riproduzione corrente per un video
     * @param url URL univoco del video
     * @param position Posizione in secondi
     * @param duration Durata totale del video in secondi
     */
    static void savePosition(const std::string& url, int64_t position, int64_t duration) {
        // Non salvare se la posizione è troppo vicina all'inizio (< 5 secondi)
        if (position < 5) {
            brls::Logger::debug("PlaybackPosition: Position too early, not saving");
            return;
        }
        
        // Non salvare se siamo troppo vicini alla fine (< 30 secondi dalla fine)
        if (duration > 0 && (duration - position) < 30) {
            brls::Logger::debug("PlaybackPosition: Position too close to end, not saving");
            return;
        }
        
        try {
            nlohmann::json data = loadCache();
            
            // Salva timestamp, posizione e durata
            data[url] = {
                {"position", position},
                {"duration", duration},
                {"timestamp", std::chrono::system_clock::now().time_since_epoch().count()}
            };
            
            saveCache(data);
            brls::Logger::info("PlaybackPosition: Saved position {} for URL: {}", position, url);
        } catch (const std::exception& e) {
            brls::Logger::error("PlaybackPosition: Error saving position: {}", e.what());
        }
    }
    
    /**
     * Recupera la posizione salvata per un video
     * @param url URL univoco del video
     * @return Posizione salvata in secondi, 0 se non trovata o scaduta
     */
    static int64_t getPosition(const std::string& url) {
        try {
            nlohmann::json data = loadCache();
            
            if (!data.contains(url)) {
                return 0;
            }
            
            auto entry = data[url];
            
            // Verifica se la posizione è ancora valida (non più vecchia di 30 giorni)
            if (entry.contains("timestamp")) {
                auto savedTime = std::chrono::system_clock::time_point(
                    std::chrono::system_clock::duration(entry["timestamp"].get<int64_t>())
                );
                auto now = std::chrono::system_clock::now();
                auto daysPassed = std::chrono::duration_cast<std::chrono::hours>(now - savedTime).count() / 24;
                
                if (daysPassed > 30) {
                    brls::Logger::debug("PlaybackPosition: Position expired for URL: {}", url);
                    return 0;
                }
            }
            
            int64_t position = entry["position"].get<int64_t>();
            brls::Logger::info("PlaybackPosition: Retrieved position {} for URL: {}", position, url);
            return position;
        } catch (const std::exception& e) {
            brls::Logger::error("PlaybackPosition: Error getting position: {}", e.what());
            return 0;
        }
    }
    
    /**
     * Rimuove la posizione salvata per un video
     * @param url URL univoco del video
     */
    static void clearPosition(const std::string& url) {
        try {
            nlohmann::json data = loadCache();
            
            if (data.contains(url)) {
                data.erase(url);
                saveCache(data);
                brls::Logger::info("PlaybackPosition: Cleared position for URL: {}", url);
            }
        } catch (const std::exception& e) {
            brls::Logger::error("PlaybackPosition: Error clearing position: {}", e.what());
        }
    }
    
    /**
     * Pulisce tutte le posizioni scadute (> 30 giorni)
     */
    static void cleanupExpiredPositions() {
        try {
            nlohmann::json data = loadCache();
            auto now = std::chrono::system_clock::now();
            
            std::vector<std::string> toRemove;
            for (auto& [url, entry] : data.items()) {
                if (entry.contains("timestamp")) {
                    auto savedTime = std::chrono::system_clock::time_point(
                        std::chrono::system_clock::duration(entry["timestamp"].get<int64_t>())
                    );
                    auto daysPassed = std::chrono::duration_cast<std::chrono::hours>(now - savedTime).count() / 24;
                    
                    if (daysPassed > 30) {
                        toRemove.push_back(url);
                    }
                }
            }
            
            for (const auto& url : toRemove) {
                data.erase(url);
            }
            
            if (!toRemove.empty()) {
                saveCache(data);
                brls::Logger::info("PlaybackPosition: Cleaned up {} expired positions", toRemove.size());
            }
        } catch (const std::exception& e) {
            brls::Logger::error("PlaybackPosition: Error cleaning up positions: {}", e.what());
        }
    }
    
private:
    static std::string getCachePath() {
        return ProgramConfig::instance().getConfigDir() + "/playback_positions.json";
    }
    
    static nlohmann::json loadCache() {
        std::string cachePath = getCachePath();
        
        std::ifstream file(cachePath);
        if (!file.is_open()) {
            return nlohmann::json::object();
        }
        
        try {
            nlohmann::json data;
            file >> data;
            file.close();
            return data;
        } catch (const std::exception& e) {
            brls::Logger::warning("PlaybackPosition: Error loading cache: {}", e.what());
            file.close();
            return nlohmann::json::object();
        }
    }
    
    static void saveCache(const nlohmann::json& data) {
        std::string cachePath = getCachePath();
        
        std::ofstream file(cachePath);
        if (!file.is_open()) {
            brls::Logger::error("PlaybackPosition: Cannot open cache file for writing");
            return;
        }
        
        file << data.dump(2);
        file.close();
    }
};

} // namespace tsvitch
