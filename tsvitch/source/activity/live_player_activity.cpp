#include <borealis/core/thread.hpp>
#include <borealis/views/dialog.hpp>

#include "activity/live_player_activity.hpp"
#include "utils/number_helper.hpp"
#include "core/DownloadManager.hpp"
#include "core/DownloadProgressManager.hpp"

#include <vector>
#include <chrono>
#include <algorithm>
#include <fmt/format.h>

#include "tsvitch.h"

#include "utils/shader_helper.hpp"
#include "utils/config_helper.hpp"
#include "utils/stream_helper.hpp"
#include "utils/playback_position_manager.hpp"

#include "view/video_view.hpp"

#include "view/grid_dropdown.hpp"
#include "view/qr_image.hpp"
#include "view/mpv_core.hpp"

#include "config/server_config.h"

#include "core/FavoriteManager.hpp"
#include "core/DownloadManager.hpp"

using namespace brls::literals;

LiveActivity::LiveActivity(const std::vector<tsvitch::LiveM3u8>& channels, size_t startIndex,
                           std::function<void()> onClose)
    : onCloseCallback(onClose), channelList(channels), currentChannelIndex(startIndex) {
    this->liveData = channelList[currentChannelIndex];
    brls::Logger::debug("LiveActivity: create: {}", liveData.title);
    ShaderHelper::instance().clearShader(false);
}

void LiveActivity::onContentAvailable() {
    brls::Logger::debug("LiveActivity: onContentAvailable");

    // Ottieni i riferimenti agli elementi UI
    video = dynamic_cast<VideoView*>(this->getView("video"));

    MPVCore::instance().setAspect(
        ProgramConfig::instance().getSettingItem(SettingItem::PLAYER_ASPECT, std::string{"-1"}));

    this->video->registerAction("", brls::BUTTON_B, [this](...) {
        if (this->video->isOSDLock()) {
            this->video->toggleOSD();
        } else {
            if (this->video->getTvControlMode() && this->video->isOSDShown()) {
                this->video->toggleOSD();
                return true;
            }
            brls::Logger::debug("exit live");
            brls::Application::popActivity();
        }
        return true;
    });

    this->video->registerAction("hints/toggle_favorite"_i18n, brls::BUTTON_X, [this](...) {
        this->video->toggleFavorite();
        return true;
    });

    this->video->registerAction("Scarica video", brls::BUTTON_Y, [this](...) {
        this->startDownload();
        return true;
    });

    //Button R go to next channel
    this->video->registerAction("hints/next_channel"_i18n, brls::BUTTON_RB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex + 1 < channelList.size()) {
                    this->video->stop();

                    currentChannelIndex++;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string& adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    //exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });
    //Button L go to previous channel
    this->video->registerAction("hints/previous_channel"_i18n, brls::BUTTON_LB, [this](...) {
        if (!this->isAd) {
            if (this->video->isOSDLock()) {
                this->video->toggleOSD();
            } else {
                if (currentChannelIndex > 0) {
                    this->video->stop();

                    currentChannelIndex--;
                    this->liveData = channelList[currentChannelIndex];
                    this->video->setTitle(liveData.title);
                    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
                    this->getAdUrlFromServer([&](const std::string& adUrl) {
                        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                        if (!adUrl.empty()) {
                            this->startAd(adUrl);
                        } else {
                            this->startLive();
                        }
                    });
                } else {
                    //exit live
                    brls::Logger::debug("exit live");
                    brls::Application::popActivity();
                }
            }
        }
        return true;
    });

    this->video->hideSubtitleSetting();
    this->video->hideVideoRelatedSetting();
    this->video->hideBottomLineSetting();
    this->video->hideHighlightLineSetting();
    this->video->disableCloseOnEndOfFile();
    this->video->setFullscreenIcon(true);
    this->video->setTitle(liveData.title);
    this->video->setFavoriteIcon(FavoriteManager::get()->isFavorite(liveData.url));
    this->video->setStatusLabelLeft("");
    this->video->setFavoriteCallback([this](bool state) { FavoriteManager::get()->toggle(this->liveData); });

    this->getAdUrlFromServer([&](const std::string& adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->startAd(adUrl);
        } else {
            this->startLive();
        }
    });

    GA("open_live", {
        {"title", this->liveData.title},
        {"url", this->liveData.url},
        {"group", this->liveData.groupTitle},
        {"index", std::to_string(currentChannelIndex)},
    });
}
void LiveActivity::startAd(std::string adUrl) {
    brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
    this->isAd = true;
    this->video->setAdMode();
    this->video->showVideoProgressSlider();
    this->video->disableProgressSliderSeek(true); // Disabilita il seek durante gli annunci
    this->video->setUrl(adUrl);

    this->video->setOnEndCallback([this]() { this->startLive(); });
}

void LiveActivity::startLive() {
    this->isAd = false;
    
    // Rileva il tipo di contenuto PRIMA di caricare l'URL
    // Questo imposta correttamente isLiveMode per eventuali errori di caricamento
    this->detectContentType();
    
    // Riabilita il seek quando non è più un annuncio
    this->video->disableProgressSliderSeek(false);
    
    this->video->setCustomToggleAction([this]() {
        if (MPVCore::instance().isStopped()) {
            this->onLiveData(this->liveData.url);
        } else if (MPVCore::instance().isPaused()) {
            MPVCore::instance().resume();
        } else {
            this->video->showOSD(false);
            MPVCore::instance().pause();
            brls::cancelDelay(toggleDelayIter);
            ASYNC_RETAIN
            toggleDelayIter = brls::delay(5000, [ASYNC_TOKEN]() {
                ASYNC_RELEASE
                if (MPVCore::instance().isPaused()) {
                    MPVCore::instance().stop();
                }
            });
        }
    });
    
    this->video->setUrl(liveData.url);
    
    // Registra un listener per l'evento MPV_LOADED per ri-verificare il tipo con la durata effettiva
    // e per ripristinare la posizione salvata
    this->tl_event_id = MPVCore::instance().getEvent()->subscribe([this](MpvEventEnum event) {
        if (event == MPV_LOADED) {
            // Ri-verifica il tipo di contenuto con la durata effettiva da MPV
            this->detectContentType();
            
            // Ripristina la posizione salvata solo per video on-demand (non per live stream)
            if (!tsvitch::isLiveStream(liveData.url, liveData.title)) {
                int64_t savedPosition = tsvitch::PlaybackPositionManager::getPosition(liveData.url);
                if (savedPosition > 0) {
                    MPVCore::instance().seek(savedPosition);
                    brls::Logger::info("LiveActivity: Restored playback position to {} seconds", savedPosition);
                }
            }
        }
    });
    mpvEventRegistered = true;
}

void LiveActivity::detectContentType() {
    // Prima fase: analisi basata su URL e titolo (disponibile sempre)
    std::string url = liveData.url;
    std::string title = liveData.title;
    
    // Converti in lowercase per il confronto
    std::transform(url.begin(), url.end(), url.begin(), ::tolower);
    std::transform(title.begin(), title.end(), title.begin(), ::tolower);
    
    // Determina il tipo basandosi su URL e titolo
    bool isLiveStream = true; // Default: assume live stream
    
    // Indicatori di video on-demand negli URL
    if (url.find(".mp4") != std::string::npos ||
        url.find(".mkv") != std::string::npos ||
        url.find(".avi") != std::string::npos ||
        url.find("video") != std::string::npos) {
        isLiveStream = false;
    }
    // Indicatori di live stream negli URL e titoli
    else if (url.find("live") != std::string::npos || 
             url.find("stream") != std::string::npos ||
             url.find(".m3u8") != std::string::npos ||
             url.find(".ts") != std::string::npos ||
             title.find("live") != std::string::npos ||
             title.find("diretta") != std::string::npos) {
        isLiveStream = true;
    }
    
    // Seconda fase: verifica con durata MPV (se disponibile)
    double duration = MPVCore::instance().duration;
    brls::Logger::debug("LiveActivity: detectContentType - duration: {}", duration);
    
    // Se MPV è già caricato, usa la durata per verificare/correggere la detection
    if (duration > 0) {
        // Se ha durata definita, è sicuramente un video
        isLiveStream = false;
        brls::Logger::debug("LiveActivity: MPV duration {} confirms VIDEO mode", duration);
    } else if (duration == 0 && MPVCore::instance().isPlaying()) {
        // Se sta riproducendo ma durata è 0, conferma che è live
        isLiveStream = true;
        brls::Logger::debug("LiveActivity: MPV duration 0 confirms LIVE mode");
    }
    // Altrimenti mantiene la detection basata su URL/titolo fatta prima
    
    brls::Logger::debug("LiveActivity: Content detected as: {}", isLiveStream ? "LIVE STREAM" : "VIDEO WITH DURATION");
    
    // Configura l'interfaccia in base al tipo di contenuto
    if (isLiveStream) {
        this->video->setLiveMode();
        this->video->hideVideoProgressSlider();
        brls::Logger::debug("LiveActivity: Configured for live stream mode");
    } else {
        this->video->setVideoMode();
        this->video->showVideoProgressSlider();
        brls::Logger::debug("LiveActivity: Configured for video mode with progress bar");
    }
}

void LiveActivity::startDownload() {
    if (this->isAd) {
        brls::Logger::debug("LiveActivity: Cannot download ads");
        return;
    }
    
    if (hasActiveDownload) {
        brls::Logger::debug("LiveActivity: Download already in progress");
        return;
    }
    
    // Controlla se è una live stream in corso
    if (tsvitch::isLiveStream(this->liveData.url, this->liveData.title)) {
        brls::Logger::warning("LiveActivity: Cannot download live streams");
        tsvitch::showLiveStreamDownloadError();
        return;
    }
    
    // Debug: stampa i dati del download
    brls::Logger::debug("LiveActivity: Starting download for title='{}', url='{}'", 
                       this->liveData.title, this->liveData.url);
    
    // Verifica che l'URL sia valido
    if (this->liveData.url.empty()) {
        brls::Logger::error("LiveActivity: Cannot download - URL is empty");
        return;
    }
    
    // Mostra l'overlay del progresso globale
    tsvitch::DownloadProgressManager::getInstance()->showDownloadProgress(
        "live_" + this->liveData.title, 
        this->liveData.title, 
        this->liveData.url
    );
    
    // Avvia il download del video corrente con callback
    currentDownloadId = DownloadManager::instance().startDownload(
        this->liveData.title, 
        this->liveData.url,
        this->liveData.logo,  // Usa il logo del canale come immagine
        [this](const std::string& downloadId, float progress, size_t downloaded, size_t total) {
            // Callback di progresso - aggiorna l'overlay globale
            if (!hasActiveDownload) {
                return;  // Download annullato o completato
            }
            
            try {
                std::string progressText;
                std::string statusText = "Download in corso...";
                
                if (total > 0) {
                    std::string downloadedStr = formatFileSize(downloaded);
                    std::string totalStr = formatFileSize(total);
                    progressText = fmt::format("{:.1f}% ({}/{})", progress, downloadedStr, totalStr);
                } else {
                    progressText = fmt::format("{:.1f}%", progress);
                }
                
                // Aggiorna l'overlay globale
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, 
                    progress, 
                    statusText, 
                    progressText
                );
                
            } catch (const std::exception& e) {
                brls::Logger::error("Error in download progress callback: {}", e.what());
                hasActiveDownload = false;
            }
        },
        [this](const std::string& downloadId, const std::string& filePath) {
            // Callback di completamento
            brls::Logger::info("Download completed: {}", downloadId);
            
            hasActiveDownload = false;
            
            try {
                // Aggiorna l'overlay globale con stato di completamento
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, 
                    100.0f, 
                    "Download completato!", 
                    "100%"
                );
                
                // Nascondi l'overlay dopo 2 secondi
                brls::delay(2000, [this]() {
                    tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress("live_" + this->liveData.title);
                });
                
                // Mostra notifica di successo
                std::string message = fmt::format("Download di \"{}\" completato", this->liveData.title);
                brls::sync([message]() {
                    brls::Dialog* dialog = new brls::Dialog(message);
                    dialog->addButton("OK", []() {});
                    dialog->open();
                });
            } catch (const std::exception& e) {
                brls::Logger::error("Error in download completion callback: {}", e.what());
            }
        },
        [this](const std::string& downloadId, const std::string& error) {
            // Callback di errore
            brls::Logger::error("Download failed: {} - {}", downloadId, error);
            
            hasActiveDownload = false;
            
            try {
                // Aggiorna l'overlay globale con stato di errore
                tsvitch::DownloadProgressManager::getInstance()->updateProgress(
                    "live_" + this->liveData.title, 
                    0.0f, 
                    "Errore nel download", 
                    "Fallito"
                );
                
                // Nascondi l'overlay dopo 3 secondi
                brls::delay(3000, [this]() {
                    tsvitch::DownloadProgressManager::getInstance()->hideDownloadProgress("live_" + this->liveData.title);
                });
                
                // Mostra notifica di errore
                std::string message = fmt::format("Errore nel download di \"{}\": {}", this->liveData.title, error);
                brls::sync([message]() {
                    brls::Dialog* dialog = new brls::Dialog(message);
                    dialog->addButton("OK", []() {});
                    dialog->open();
                });
            } catch (const std::exception& e) {
                brls::Logger::error("Error in download error callback: {}", e.what());
            }
        }
    );
    
    if (!currentDownloadId.empty()) {
        hasActiveDownload = true;
        brls::Logger::info("LiveActivity: Started download {} for {}", currentDownloadId, this->liveData.title);
    } else {
        // Download fallito immediatamente
        hasActiveDownload = false;
        
        brls::Dialog* dialog = new brls::Dialog("Impossibile avviare il download");
        dialog->addButton("OK", []() {});
        dialog->open();
    }
}

void LiveActivity::onLiveData(std::string url) {
    brls::Logger::debug("Live stream url: {}", url);
    this->getAdUrlFromServer([&](const std::string& adUrl) {
        brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
        if (!adUrl.empty()) {
            this->video->setUrl(adUrl);
        } else {
            this->startLive();
        }
    });
    return;
}

void LiveActivity::onError(const std::string& error) {
    brls::Logger::error("ERROR request live data: {}", error);
    this->video->showOSD(false);
    this->retryRequestData();
}

void LiveActivity::retryRequestData() {
    brls::cancelDelay(errorDelayIter);
    errorDelayIter = brls::delay(2000, [this]() {
        if (!MPVCore::instance().isPlaying()) static_cast<LiveDataRequest*>(this)->requestData(liveData.url);
    });
}

void LiveActivity::getAdUrlFromServer(std::function<void(const std::string&)> callback) {
    CLIENT::get_ad(
        [callback](const std::string& adUrl, int statusCode) {
            if (statusCode == 200 && !adUrl.empty()) {
                brls::Logger::debug("LiveActivity: adUrl: {}", adUrl);
                if (callback) callback(adUrl);
            } else {
                brls::Logger::error("LiveActivity: Failed to get ad URL, status code: {}", statusCode);
                if (callback) callback("");
            }
        },
        [callback](const std::string& error, int statusCode) {
            brls::Logger::error("LiveActivity: Error getting ad URL: {}, status code: {}", error, statusCode);
            if (callback) callback("");
        });
}

std::string LiveActivity::formatFileSize(size_t bytes) {
    const char* suffixes[] = {"B", "KB", "MB", "GB"};
    int suffixIndex = 0;
    double size = static_cast<double>(bytes);
    
    while (size >= 1024 && suffixIndex < 3) {
        size /= 1024;
        suffixIndex++;
    }
    
    if (suffixIndex == 0) {
        return fmt::format("{} {}", static_cast<int>(size), suffixes[suffixIndex]);
    } else {
        return fmt::format("{:.1f} {}", size, suffixes[suffixIndex]);
    }
}

LiveActivity::~LiveActivity() {
    brls::Logger::debug("LiveActivity: delete");
    
    // Salva la posizione di riproduzione prima di uscire (solo per video on-demand)
    if (!tsvitch::isLiveStream(liveData.url, liveData.title)) {
        int64_t currentPosition = MPVCore::instance().playback_time;
        int64_t totalDuration = MPVCore::instance().duration;
        
        if (currentPosition > 0 && totalDuration > 0) {
            tsvitch::PlaybackPositionManager::savePosition(liveData.url, currentPosition, totalDuration);
            brls::Logger::info("LiveActivity: Saved playback position {} / {}", currentPosition, totalDuration);
        }
    }
    
    // Cancella download attivo se presente
    if (hasActiveDownload && !currentDownloadId.empty()) {
        brls::Logger::debug("LiveActivity: Cancelling active download {}", currentDownloadId);
        DownloadManager::instance().cancelDownload(currentDownloadId);
        hasActiveDownload = false;
        currentDownloadId.clear();
    }
    
    if (this->video) {
        this->video->setOnEndCallback(nullptr);  // Annulla la callback per evitare crash
        this->video->stop();
    }
    brls::cancelDelay(toggleDelayIter);
    brls::cancelDelay(errorDelayIter);
    
    // Pulisci gli eventi in modo sicuro per evitare callback dopo la distruzione
    try {
        if (mpvEventRegistered) {
            MPVCore::instance().getEvent()->unsubscribe(this->tl_event_id);
            mpvEventRegistered = false;
        }
    } catch (const std::exception& e) {
        brls::Logger::warning("LiveActivity: Error unsubscribing MPV event: {}", e.what());
    } catch (...) {
        brls::Logger::warning("LiveActivity: Unknown error unsubscribing MPV event");
    }
    
    try {
        if (customEventRegistered) {
            EventHelper::instance().getCustomEvent()->unsubscribe(this->event_id);
            customEventRegistered = false;
        }
    } catch (const std::exception& e) {
        brls::Logger::warning("LiveActivity: Error unsubscribing custom event: {}", e.what());
    } catch (...) {
        brls::Logger::warning("LiveActivity: Unknown error unsubscribing custom event");
    }
    
    if (onCloseCallback) onCloseCallback();
}