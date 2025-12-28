#pragma once

#include <string>
#include <algorithm>
#include <borealis/core/i18n.hpp>
#include <borealis/views/dialog.hpp>

namespace tsvitch {

/**
 * Determina se un contenuto è una live stream basandosi su URL e titolo
 * @param url URL del contenuto
 * @param title Titolo del contenuto
 * @return true se è una live stream, false altrimenti
 */
inline bool isLiveStream(const std::string& url, const std::string& title) {
    // Converte tutto in minuscolo per confronto case-insensitive
    std::string urlLower = url;
    std::string titleLower = title;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    std::transform(titleLower.begin(), titleLower.end(), titleLower.begin(), ::tolower);
    
    // Indicatori di live stream negli URL e titoli
    if (urlLower.find("live") != std::string::npos || 
        urlLower.find("stream") != std::string::npos ||
        urlLower.find(".m3u8") != std::string::npos ||
        urlLower.find(".ts") != std::string::npos ||
        titleLower.find("live") != std::string::npos ||
        titleLower.find("diretta") != std::string::npos) {
        return true;
    }
    
    return false;
}

/**
 * Mostra un dialog di errore per il download di live stream
 */
inline void showLiveStreamDownloadError() {
    // Dialog accetta solo una stringa, quindi concateniamo titolo e descrizione
    std::string message = brls::getStr("tsvitch/cast/live_download_error") + "\n\n" +
                          brls::getStr("tsvitch/cast/live_download_error_desc");
    
    brls::Dialog* dialog = new brls::Dialog(message);
    dialog->addButton(brls::getStr("brls/hints/ok"), []() {});
    dialog->open();
}

} // namespace tsvitch
