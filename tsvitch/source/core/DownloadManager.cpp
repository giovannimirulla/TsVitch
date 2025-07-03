#include "core/DownloadManager.hpp"
#include "utils/config_helper.hpp"
#include <borealis/core/logger.hpp>
#include <borealis/core/application.hpp>
#include <fstream>
#include <cstdio>
#include <filesystem>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <atomic>

using json = nlohmann::json;

// Callback per scrivere i dati scaricati
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::ofstream* stream) {
    size_t totalSize = size * nmemb;
    stream->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback per il progresso del download
static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    struct ProgressData {
        DownloadManager* manager;
        std::string downloadId;
        size_t existingFileSize; // Dimensione del file esistente per il resume
        std::chrono::steady_clock::time_point lastCallbackTime; // Per throttling
        float lastProgress = -1.0f; // Ultimo progresso inviato per evitare aggiornamenti inutili
    };
    
    auto* data = static_cast<ProgressData*>(clientp);
    if (!data || !data->manager) {
        return 0; // Manager non valido, esci
    }

    // Controlla se il manager è in fase di shutdown
    if (data->manager->shouldStop.load()) {
        return 1; // Interrompe il download
    }
    
    if (dltotal > 0) {
        // Calcola il progresso considerando il file esistente per il resume
        curl_off_t totalDownloaded = dlnow + data->existingFileSize;
        curl_off_t totalExpected = dltotal + data->existingFileSize;
        
        // Se stiamo facendo un resume, dltotal potrebbe essere solo la parte rimanente
        // In questo caso, cerchiamo di ottenere la dimensione totale dal download manager
        {
            std::lock_guard<std::mutex> lock(data->manager->downloadsMutex);
            auto it = data->manager->findDownload(data->downloadId);
            if (it != data->manager->downloads.end()) {
                // Se abbiamo già una totalSize valida dal download manager, usala
                if (it->totalSize > 0 && it->totalSize > totalExpected) {
                    totalExpected = it->totalSize;
                }
            }
        }
        
        float progress = static_cast<float>(totalDownloaded) / static_cast<float>(totalExpected) * 100.0f;
        size_t downloaded = static_cast<size_t>(totalDownloaded);
        size_t total = static_cast<size_t>(totalExpected);
        
        // Limita il progresso al 100%
        if (progress > 100.0f) {
            progress = 100.0f;
        }
        
        // Throttling: aggiorna i callback solo se è passato abbastanza tempo O se il progresso è cambiato significativamente
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastCallback = std::chrono::duration_cast<std::chrono::milliseconds>(now - data->lastCallbackTime);
        float progressDiff = std::abs(progress - data->lastProgress);
        
        bool shouldUpdate = (timeSinceLastCallback.count() >= 250) || // Massimo 4 volte al secondo
                           (progressDiff >= 1.0f) || // Solo se il progresso è cambiato di almeno 1%
                           (progress >= 100.0f && data->lastProgress < 100.0f); // Sempre aggiorna al completamento
        
        if (!shouldUpdate) {
            return 0; // Skip questo aggiornamento
        }
        
        data->lastCallbackTime = now;
        data->lastProgress = progress;
        
        // Aggiorna l'item nel manager
        {
            std::lock_guard<std::mutex> lock(data->manager->downloadsMutex);
            auto it = data->manager->findDownload(data->downloadId);
            if (it != data->manager->downloads.end()) {
                it->progress = progress;
                it->downloadedSize = downloaded;
                it->totalSize = total;
            }
        }
        
        // Chiama il callback specifico per questo download se presente (thread-safe)
        DownloadManager::DownloadProgressCallback specificCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(data->manager->downloadsMutex);
            auto progressIt = data->manager->downloadProgressCallbacks.find(data->downloadId);
            if (progressIt != data->manager->downloadProgressCallbacks.end()) {
                specificCallback = progressIt->second; // Copia il callback
            }
        }
        
        if (specificCallback) {
            specificCallback(data->downloadId, progress, downloaded, total);
        }
        
        // Chiama il callback globale se presente
        DownloadManager::GlobalProgressCallback globalCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(data->manager->downloadsMutex);
            if (data->manager->globalProgressCallback) {
                globalCallback = data->manager->globalProgressCallback; // Copia il callback
            }
        } // Il lock viene rilasciato automaticamente qui
        
        if (globalCallback) {
            globalCallback(data->downloadId, progress, downloaded, total);
        }
    }
    return 0;
}

std::string DownloadManager::startDownload(const std::string& title, const std::string& url) {
    return startDownload(title, url, "", nullptr, nullptr, nullptr);
}

std::string DownloadManager::startDownload(const std::string& title, const std::string& url, const std::string& imageUrl) {
    return startDownload(title, url, imageUrl, nullptr, nullptr, nullptr);
}

std::string DownloadManager::startDownload(const std::string& title, const std::string& url,
                                          DownloadProgressCallback progressCallback,
                                          DownloadCompleteCallback completeCallback,
                                          DownloadErrorCallback errorCallback) {
    return startDownload(title, url, "", progressCallback, completeCallback, errorCallback);
}

std::string DownloadManager::startDownload(const std::string& title, const std::string& url, const std::string& imageUrl,
                                          DownloadProgressCallback progressCallback,
                                          DownloadCompleteCallback completeCallback,
                                          DownloadErrorCallback errorCallback) {
    // Pulisci l'URL da caratteri di whitespace
    std::string cleanUrl = url;
    // Rimuovi spazi, tab, newline, carriage return all'inizio e alla fine
    cleanUrl.erase(0, cleanUrl.find_first_not_of(" \t\n\r"));
    cleanUrl.erase(cleanUrl.find_last_not_of(" \t\n\r") + 1);
    
    // Validazione URL
    if (cleanUrl.empty()) {
        brls::Logger::error("DownloadManager: Cannot start download - URL is empty");
        if (errorCallback) {
            errorCallback("", "URL is empty");
        }
        return "";
    }
    
    // Validazione URL basic (deve iniziare con http/https)
    if (cleanUrl.find("http://") != 0 && cleanUrl.find("https://") != 0) {
        brls::Logger::error("DownloadManager: Cannot start download - Invalid URL format: {}", cleanUrl);
        if (errorCallback) {
            errorCallback("", "Invalid URL format");
        }
        return "";
    }
    
    brls::Logger::debug("DownloadManager: Starting download with cleaned URL: '{}'", cleanUrl);
    
    std::lock_guard<std::mutex> lock(downloadsMutex);
    
    std::string id = generateDownloadId();
    DownloadItem item;
    item.id = id;
    item.title = title;
    item.url = cleanUrl; // Usa l'URL pulito
    item.imageUrl = imageUrl; // Aggiungi l'URL dell'immagine
    item.status = DownloadStatus::PENDING;
    item.progress = 0.0f;
    
    // Genera il path locale - la directory verrà creata automaticamente quando necessario
    std::string filename = title + "_" + id + ".mp4"; // Assumiamo formato MP4
    // Rimuovi caratteri non validi dal filename
    std::replace_if(filename.begin(), filename.end(), [](char c) {
        return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|';
    }, '_');
    item.localPath = getDownloadDirectory() + "/" + filename;
    
    downloads.push_back(item);
    
    // Registra i callback specifici per questo download
    if (progressCallback) {
        downloadProgressCallbacks[id] = progressCallback;
    }
    if (completeCallback) {
        downloadCompleteCallbacks[id] = completeCallback;
    }
    if (errorCallback) {
        downloadErrorCallbacks[id] = errorCallback;
    }
    
    // Avvia il thread di download
    downloadThreads.emplace_back(&DownloadManager::downloadWorker, this, id);
    
    brls::Logger::info("DownloadManager: Started download {} for {}", id, title);
    saveDownloads();
    
    return id;
}

void DownloadManager::pauseDownload(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it != downloads.end() && it->status == DownloadStatus::DOWNLOADING) {
        it->status = DownloadStatus::PAUSED;
        brls::Logger::info("DownloadManager: Paused download {}", id);
        saveDownloads();
    }
}

void DownloadManager::resumeDownload(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it != downloads.end() && it->status == DownloadStatus::PAUSED) {
        it->status = DownloadStatus::DOWNLOADING;
        // Avvia un nuovo thread per il download
        downloadThreads.emplace_back(&DownloadManager::downloadWorker, this, id);
        brls::Logger::info("DownloadManager: Resumed download {}", id);
        saveDownloads();
    }
}

void DownloadManager::cancelDownload(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it != downloads.end()) {
        // Rimuovi il file parziale se esiste usando std::ifstream per il controllo
        std::ifstream testFile(it->localPath);
        if (testFile.good()) {
            testFile.close();
            if (std::remove(it->localPath.c_str()) == 0) {
                brls::Logger::info("DownloadManager: Removed file {}", it->localPath);
            } else {
                brls::Logger::warning("DownloadManager: Failed to remove file {}", it->localPath);
            }
        }
        
        // Rimuovi il download dalla lista invece di impostarlo come cancellato
        downloads.erase(it);
        brls::Logger::info("DownloadManager: Cancelled and removed download {}", id);
        saveDownloads();
    }
}

void DownloadManager::deleteDownload(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it != downloads.end()) {
        // Rimuovi il file se esiste usando std::ifstream per il controllo
        std::ifstream testFile(it->localPath);
        if (testFile.good()) {
            testFile.close();
            if (std::remove(it->localPath.c_str()) == 0) {
                brls::Logger::info("DownloadManager: Removed file {}", it->localPath);
            } else {
                brls::Logger::warning("DownloadManager: Failed to remove file {}", it->localPath);
            }
        }
        downloads.erase(it);
        brls::Logger::info("DownloadManager: Deleted download {}", id);
        saveDownloads();
    }
}

std::vector<DownloadItem> DownloadManager::getAllDownloads() const {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    brls::Logger::debug("DownloadManager::getAllDownloads - returning {} downloads", downloads.size());
    for (const auto& download : downloads) {
        brls::Logger::debug("  Download: {} - {} - {:.1f}%", download.id, download.title, download.progress);
    }
    return downloads;
}

DownloadItem DownloadManager::getDownload(const std::string& id) const {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it != downloads.end()) {
        return *it;
    }
    return DownloadItem{}; // Ritorna un item vuoto se non trovato
}

std::string DownloadManager::getDownloadDirectory() const {
    const std::string configDir = ProgramConfig::instance().getConfigDir();
    return configDir + "/downloads";
}

void DownloadManager::downloadWorker(const std::string& id) {
    // Verifica che il manager non sia in fase di shutdown
    if (shouldStop.load()) {
        brls::Logger::debug("DownloadManager: Worker {} exiting due to shutdown", id);
        return;
    }
    
    // Verifica che il download esista ancora
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it == downloads.end()) {
            brls::Logger::warning("DownloadManager: Download {} not found in worker, exiting", id);
            return;
        }
        it->status = DownloadStatus::DOWNLOADING;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end()) {
                it->status = DownloadStatus::FAILED;
                it->error = "Failed to initialize CURL";
            }
        }
        
        // Chiama callback di errore
        DownloadManager::DownloadErrorCallback errorCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto errorIt = downloadErrorCallbacks.find(id);
            if (errorIt != downloadErrorCallbacks.end()) {
                errorCallback = errorIt->second; // Copia il callback
            }
        }
        
        if (errorCallback) {
            errorCallback(id, "Failed to initialize CURL");
        }
        return;
    }
    
    DownloadItem item;
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it == downloads.end()) {
            curl_easy_cleanup(curl);
            return;
        }
        item = *it;
    }
    
    // Verifica se il file esiste già per un possibile resume
    size_t existingFileSize = 0;
    bool resumeDownload = false;
    
    // Usa ifstream per verificare se il file esiste e ottenere la dimensione
    std::ifstream checkFile(item.localPath, std::ios::binary | std::ios::ate);
    if (checkFile.is_open()) {
        existingFileSize = static_cast<size_t>(checkFile.tellg());
        checkFile.close();
        
        if (existingFileSize > 0) {
            resumeDownload = true;
            brls::Logger::info("DownloadManager: Found existing file with {} bytes, attempting resume", existingFileSize);
        }
    }
    
    // Se stiamo facendo un resume, aggiorniamo il progresso nel download manager prima di iniziare
    if (resumeDownload) {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            // Imposta i valori di progresso iniziali per il resume
            it->downloadedSize = existingFileSize;
            // La totalSize verrà aggiornata quando conosceremo il Content-Length
        }
    }
    
    // La directory di download verrà creata automaticamente se necessario
    
    // Apri il file in modalità appropriata
    std::ofstream outFile;
    if (resumeDownload) {
        outFile.open(item.localPath, std::ios::binary | std::ios::app);
    } else {
        outFile.open(item.localPath, std::ios::binary);
    }
    
    if (!outFile.is_open()) {
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end()) {
                it->status = DownloadStatus::FAILED;
                it->error = "Failed to open output file";
            }
        }
        
        curl_easy_cleanup(curl);
        
        // Chiama callback di errore
        DownloadManager::DownloadErrorCallback errorCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto errorIt = downloadErrorCallbacks.find(id);
            if (errorIt != downloadErrorCallbacks.end()) {
                errorCallback = errorIt->second; // Copia il callback
            }
        }
        
        if (errorCallback) {
            errorCallback(id, "Failed to open output file");
        }
        return;
    }
    
    // Struttura per passare dati al callback di progresso
    struct ProgressData {
        DownloadManager* manager;
        std::string downloadId;
        size_t existingFileSize; // Dimensione del file esistente per il resume
        std::chrono::steady_clock::time_point lastCallbackTime; // Per throttling
        float lastProgress = -1.0f; // Ultimo progresso inviato per evitare aggiornamenti inutili
    } progressData{this, id, existingFileSize, std::chrono::steady_clock::now(), -1.0f};
    
    // Inizializza headers
    struct curl_slist* headers = NULL;
    
    curl_easy_setopt(curl, CURLOPT_URL, item.url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &outFile);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L); // Timeout di 1 ora
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &progressData);
    
    // Se stiamo facendo un resume, imposta il range request
    if (resumeDownload && existingFileSize > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(existingFileSize));
        brls::Logger::info("DownloadManager: Setting resume from byte {}", existingFileSize);
    }
    
    // Aggiungi headers appropriati per evitare blocchi del server
    // Estrai il dominio dall'URL per il referer
    std::string referer;
    size_t protocolEnd = item.url.find("://");
    if (protocolEnd != std::string::npos) {
        size_t domainStart = protocolEnd + 3;
        size_t domainEnd = item.url.find("/", domainStart);
        if (domainEnd == std::string::npos) {
            domainEnd = item.url.find(":", domainStart);
        }
        if (domainEnd != std::string::npos) {
            std::string domain = item.url.substr(0, domainEnd);
            referer = domain + "/";
        } else {
            std::string domain = item.url.substr(0, item.url.find("?", domainStart));
            referer = domain + "/";
        }
    } else {
        // Fallback se non riusciamo a estrarre il dominio
        referer = "https://www.google.com/";
    }
    
    std::string refererHeader = "Referer: " + referer;
    headers = curl_slist_append(headers, refererHeader.c_str());
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9,it;q=0.8");
    headers = curl_slist_append(headers, "Accept-Encoding: identity");
    
    brls::Logger::info("DownloadManager: Using referer: {}", referer);
    
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // Aggiungi timeout per evitare hang durante shutdown
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L); // 30 secondi timeout totale
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L); // 10 secondi timeout connessione
    
    // Aggiungi logging dettagliato
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 0L); // Disabilita per ora il verbose
    
    brls::Logger::info("DownloadManager: Starting download from URL: {}", item.url);
    brls::Logger::info("DownloadManager: Target file: {}", item.localPath);
    
    // Controlla se dobbiamo fermarci prima di iniziare il download
    if (shouldStop.load()) {
        brls::Logger::debug("DownloadManager: Aborting download {} due to shutdown", id);
        if (headers) {
            curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);
        return;
    }
    
    CURLcode res = curl_easy_perform(curl);
    outFile.close();
    
    // Controlla il codice di risposta HTTP
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    brls::Logger::info("DownloadManager: HTTP response code: {}", httpCode);
    
    // Controlla la dimensione scaricata
    curl_off_t downloadSize = 0;
    curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloadSize);
    
    // Se stavamo facendo un resume, aggiungi la dimensione esistente
    curl_off_t totalDownloaded = downloadSize;
    if (resumeDownload) {
        totalDownloaded += existingFileSize;
    }
    
    brls::Logger::info("DownloadManager: Downloaded {} bytes (session: {}, total: {})", totalDownloaded, downloadSize, totalDownloaded);
    
    // Ottieni la dimensione del contenuto dal server (se disponibile)
    curl_off_t contentLength = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    brls::Logger::info("DownloadManager: Content-Length: {} bytes", contentLength);
    
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            // Verifica se il download è andato a buon fine
            bool downloadSuccessful = false;
            
            if (httpCode >= 200 && httpCode < 300) {
                if (res == CURLE_OK) {
                    // Download completato senza errori
                    downloadSuccessful = true;
                } else if (res == CURLE_PARTIAL_FILE && totalDownloaded > 0) {
                    // Download parziale ma con dati - verifica se è accettabile
                    if (contentLength > 0) {
                        // Se abbiamo Content-Length, verifica che sia ragionevolmente completo
                        float completionRatio = static_cast<float>(totalDownloaded) / static_cast<float>(contentLength);
                        
                        // Considera il download valido se:
                        // 1. È completamente scaricato (100%)
                        // 2. È sostanzialmente completo (>98%) per file piccoli (<100MB)
                        // 3. Ha scaricato almeno il 95% per file grandi (>100MB) - potrebbe essere stato interrotto naturalmente
                        bool isLargeFile = contentLength > (100 * 1024 * 1024); // 100MB
                        float minRatio = isLargeFile ? 0.95f : 0.98f;
                        
                        downloadSuccessful = (completionRatio >= minRatio);
                        
                        brls::Logger::info("DownloadManager: Partial download check - downloaded: {}, expected: {}, ratio: {:.2f}%, min required: {:.1f}%, large file: {}, success: {}", 
                                         totalDownloaded, contentLength, completionRatio * 100.0f, minRatio * 100.0f, isLargeFile, downloadSuccessful);
                        
                        // Se non è considerato successo, imposta come paused per permettere il resume
                        if (!downloadSuccessful && completionRatio > 0.1f) {
                            brls::Logger::info("DownloadManager: Setting partial download as PAUSED for potential resume");
                            it->status = DownloadStatus::PAUSED;
                            it->progress = completionRatio * 100.0f;
                            it->downloadedSize = totalDownloaded;
                            it->totalSize = contentLength;
                            it->error = "Download interrupted at " + std::to_string(static_cast<int>(completionRatio * 100.0f)) + "% - can be resumed";
                            
                            // Non chiamare callback di errore per download interrotti che possono essere ripresi
                            saveDownloads();
                            
                            // Pulisci i callback per questo download
                            {
                                std::lock_guard<std::mutex> callbackLock(downloadsMutex);
                                downloadProgressCallbacks.erase(id);
                                downloadCompleteCallbacks.erase(id);
                                downloadErrorCallbacks.erase(id);
                            }
                            
                            // Pulisci headers e CURL
                            if (headers) {
                                curl_slist_free_all(headers);
                            }
                            curl_easy_cleanup(curl);
                            return;
                        }
                    } else {
                        // Se non abbiamo Content-Length, accetta se abbiamo ricevuto dati
                        downloadSuccessful = true;
                        brls::Logger::info("DownloadManager: Partial download without Content-Length - accepting as valid");
                    }
                }
            }
            
            if (downloadSuccessful) {
                it->status = DownloadStatus::COMPLETED;
                it->progress = 100.0f;
                
                // Usa la dimensione che abbiamo già calcolato invece di filesystem
                it->downloadedSize = totalDownloaded;
                it->totalSize = (contentLength > 0) ? contentLength + (resumeDownload ? existingFileSize : 0) : totalDownloaded;
                
                // Se è un resume, il contentLength è solo della parte scaricata, dobbiamo aggiungere la dimensione esistente
                if (resumeDownload && contentLength > 0) {
                    it->totalSize = totalDownloaded; // Usa il totale effettivo
                }
                
                if (it->downloadedSize == 0) {
                    brls::Logger::warning("DownloadManager: Download {} completed but no data was recorded (HTTP {})", id, httpCode);
                    it->status = DownloadStatus::FAILED;
                    it->error = "No data downloaded (HTTP " + std::to_string(httpCode) + ")";
                } else {
                    brls::Logger::info("DownloadManager: Download {} completed successfully ({} bytes)", id, it->downloadedSize);
                }
                
                saveDownloads();
            } else {
                it->status = DownloadStatus::FAILED;
                std::string errorMsg = curl_easy_strerror(res);
                
                // Fornisci informazioni più dettagliate sull'errore
                if (res == CURLE_PARTIAL_FILE) {
                    if (contentLength > 0 && totalDownloaded < contentLength) {
                        errorMsg += " (" + std::to_string(totalDownloaded) + "/" + std::to_string(contentLength) + " bytes)";
                    } else {
                        errorMsg += " (downloaded " + std::to_string(totalDownloaded) + " bytes)";
                    }
                }
                
                if (httpCode >= 400) {
                    errorMsg += " (HTTP " + std::to_string(httpCode) + ")";
                }
                
                it->error = errorMsg;
                brls::Logger::error("DownloadManager: Download {} failed: {} (HTTP {})", id, errorMsg, httpCode);
                
                saveDownloads();
            }
        }
    }
    
    // Gestisci i callback fuori dal lock principale per evitare deadlock
    // Prima ottieni lo stato attuale
    bool isCompleted = false;
    bool isFailed = false;
    std::string localPath;
    std::string errorMessage;
    
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            isCompleted = (it->status == DownloadStatus::COMPLETED);
            isFailed = (it->status == DownloadStatus::FAILED);
            localPath = it->localPath;
            errorMessage = it->error;
        }
    }
    
    // Chiama i callback appropriati
    if (isCompleted) {
        // Ottieni e chiama callback di completamento
        DownloadManager::DownloadCompleteCallback completeCallback = nullptr;
        DownloadManager::GlobalCompleteCallback globalCallback = nullptr;
        
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto completeIt = downloadCompleteCallbacks.find(id);
            if (completeIt != downloadCompleteCallbacks.end()) {
                completeCallback = completeIt->second;
            }
            if (globalCompleteCallback) {
                globalCallback = globalCompleteCallback;
            }
        }
        
        // Chiama i callback fuori da tutti i lock
        if (completeCallback) {
            try {
                completeCallback(id, localPath);
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in complete callback: {}", e.what());
            }
        }
        if (globalCallback) {
            try {
                globalCallback(id, true);
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in global callback: {}", e.what());
            }
        }
    } else if (isFailed) {
        // Ottieni e chiama callback di errore
        DownloadManager::DownloadErrorCallback errorCallback = nullptr;
        DownloadManager::GlobalCompleteCallback globalCallback = nullptr;
        
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto errorIt = downloadErrorCallbacks.find(id);
            if (errorIt != downloadErrorCallbacks.end()) {
                errorCallback = errorIt->second;
            }
            if (globalCompleteCallback) {
                globalCallback = globalCompleteCallback;
            }
        }
        
        // Chiama i callback fuori da tutti i lock
        if (errorCallback) {
            try {
                errorCallback(id, errorMessage);
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in error callback: {}", e.what());
            }
        }
        if (globalCallback) {
            try {
                globalCallback(id, false);
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in global callback: {}", e.what());
            }
        }
    }
    
    // Pulisci i callback per questo download (thread-safe)
    {
        std::lock_guard<std::mutex> callbackLock(downloadsMutex);
        downloadProgressCallbacks.erase(id);
        downloadCompleteCallbacks.erase(id);
        downloadErrorCallbacks.erase(id);
    }
    
    // Pulisci headers e CURL
    if (headers) {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);
}

std::string DownloadManager::generateDownloadId() const {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    
    std::stringstream ss;
    for (int i = 0; i < 8; ++i) {
        ss << std::hex << dis(gen);
    }
    return ss.str();
}

std::vector<DownloadItem>::iterator DownloadManager::findDownload(const std::string& id) {
    return std::find_if(downloads.begin(), downloads.end(),
                       [&id](const DownloadItem& item) { return item.id == id; });
}

std::vector<DownloadItem>::const_iterator DownloadManager::findDownload(const std::string& id) const {
    return std::find_if(downloads.begin(), downloads.end(),
                       [&id](const DownloadItem& item) { return item.id == id; });
}

std::string DownloadManager::getDownloadsStatePath() const {
    return getDownloadDirectory() + "/downloads.json";
}

void DownloadManager::saveDownloads() {
    try {
        // La directory verrà creata automaticamente se necessario dal file system
        json j = json::array();
        for (const auto& download : downloads) {
            json item;
            item["id"] = download.id;
            item["title"] = download.title;
            item["url"] = download.url;
            item["localPath"] = download.localPath;
            item["status"] = static_cast<int>(download.status);
            item["progress"] = download.progress;
            item["totalSize"] = download.totalSize;
            item["downloadedSize"] = download.downloadedSize;
            item["error"] = download.error;
            item["imageUrl"] = download.imageUrl;
            j.push_back(item);
        }
        
        std::ofstream file(getDownloadsStatePath());
        file << j.dump(2);
        file.close();
    } catch (const std::exception& e) {
        brls::Logger::error("DownloadManager: Failed to save downloads state: {}", e.what());
    }
}

void DownloadManager::loadDownloads() {
    try {
        std::string statePath = getDownloadsStatePath();
        
        // Verifica che il file esista usando std::ifstream invece di std::filesystem
        std::ifstream testFile(statePath);
        if (!testFile.good()) {
            testFile.close();
            brls::Logger::info("DownloadManager: No downloads state file found at {}", statePath);
            return; // File non esiste, nessun problema
        }
        testFile.close();
        
        std::ifstream file(statePath);
        if (!file.is_open()) {
            brls::Logger::error("DownloadManager: Failed to open downloads state file: {}", statePath);
            return;
        }
        
        json j;
        file >> j;
        file.close();
        
        brls::Logger::info("DownloadManager: Loading {} downloads from state file", j.size());
        
        downloads.clear();
        for (const auto& item : j) {
            DownloadItem download;
            download.id = item["id"];
            download.title = item["title"];
            
            // Pulisci l'URL da caratteri di whitespace
            std::string cleanUrl = item["url"];
            cleanUrl.erase(0, cleanUrl.find_first_not_of(" \t\n\r"));
            cleanUrl.erase(cleanUrl.find_last_not_of(" \t\n\r") + 1);
            download.url = cleanUrl;
            
            download.localPath = item["localPath"];
            download.status = static_cast<DownloadStatus>(item["status"]);
            download.progress = item["progress"];
            download.totalSize = item["totalSize"];
            download.downloadedSize = item["downloadedSize"];
            download.error = item["error"];
            
            // Handle imageUrl field (might not exist in older saves)
            if (item.contains("imageUrl")) {
                download.imageUrl = item["imageUrl"];
            } else {
                download.imageUrl = ""; // Default to empty string for backward compatibility
            }
            
            // Reset status dei download in corso al riavvio
            if (download.status == DownloadStatus::DOWNLOADING) {
                download.status = DownloadStatus::PAUSED;
            }
            
            downloads.push_back(download);
        }
        
        brls::Logger::info("DownloadManager: Loaded {} downloads from state", downloads.size());
    } catch (const std::exception& e) {
        brls::Logger::error("DownloadManager: Failed to load downloads state: {}", e.what());
    }
}
DownloadManager::DownloadManager() {
}
DownloadManager::~DownloadManager() {
    brls::Logger::debug("DownloadManager: Starting destruction");
    shouldStop = true;
    
    // Pulisci tutti i callback prima di aspettare i thread
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        globalProgressCallback = nullptr;
        globalCompleteCallback = nullptr;
        downloadProgressCallbacks.clear();
        downloadCompleteCallbacks.clear();
        downloadErrorCallbacks.clear();
        
        // Interrompi tutti i download attivi
        for (auto& download : downloads) {
            if (download.status == DownloadStatus::DOWNLOADING || 
                download.status == DownloadStatus::PENDING) {
                download.status = DownloadStatus::PAUSED;
                brls::Logger::debug("DownloadManager: Paused download {} during shutdown", download.id);
            }
        }
    }
    
    // Aspetta che tutti i thread finiscano con timeout
    auto startTime = std::chrono::steady_clock::now();
    for (auto& thread : downloadThreads) {
        if (thread.joinable()) {
            // Aspetta massimo 2 secondi per thread
            auto elapsed = std::chrono::steady_clock::now() - startTime;
            if (elapsed < std::chrono::seconds(2)) {
                thread.join();
            } else {
                brls::Logger::warning("DownloadManager: Thread join timeout during shutdown");
                thread.detach(); // Detach se il join impiega troppo tempo
            }
        }
    }
    
    saveDownloads();
    brls::Logger::debug("DownloadManager: Destroyed");
}

void DownloadManager::setGlobalProgressCallback(GlobalProgressCallback callback) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    globalProgressCallback = callback;
    brls::Logger::info("DownloadManager: Global progress callback set");
}

void DownloadManager::setGlobalCompleteCallback(GlobalCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    globalCompleteCallback = callback;
    brls::Logger::info("DownloadManager: Global complete callback set");
}
