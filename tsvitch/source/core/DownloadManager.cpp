#include "core/DownloadManager.hpp"
#include "core/DownloadProgressManager.hpp"
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
#include <thread>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include <atomic>
#include <sys/stat.h>
#include <sys/types.h>
#include <cerrno>
#include <cstring>
#include <map>
#include <algorithm>
#include <cstdlib>
#include <sys/stat.h>
#include <climits>

#ifdef __SWITCH__
    #include <switch.h>
#else
    #include <sys/statvfs.h>
    #include <sys/socket.h>
#endif

#ifdef _WIN32
    #include <direct.h>
    #define mkdir(dir, mode) _mkdir(dir)
#else
    #include <unistd.h>
#endif

using json = nlohmann::json;

// Global retry counter for downloads to persist across function calls
static std::map<std::string, int> globalRetryCount;
static std::mutex retryCountMutex;

// Struttura semplificata per i dati di progresso (solo per downloadSimplified)
struct SimpleProgressData {
    DownloadManager* manager;
    std::string downloadId;
};

// Struttura per i dati di progresso (per il callback normale)
struct ProgressData {
    DownloadManager* manager;
    std::string downloadId;
    size_t existingFileSize; // Dimensione del file esistente per il resume
    std::chrono::steady_clock::time_point lastCallbackTime; // Per throttling
    float lastProgress = -1.0f; // Ultimo progresso inviato per evitare aggiornamenti inutili
};

// Callback per scrivere i dati scaricati
static size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::ofstream* stream) {
    size_t totalSize = size * nmemb;
    stream->write(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// Callback normale per il progresso del download (per downloadWorker)
static int ProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    ProgressData* data = static_cast<ProgressData*>(clientp);
    if (!data || !data->manager) {
        return 0;
    }

    if (data->manager->shouldStop.load()) {
        return 1;
    }
    
    if (dltotal > 0 && dlnow >= 0) {
        curl_off_t totalDownloaded = dlnow + data->existingFileSize;
        curl_off_t totalExpected = dltotal + data->existingFileSize;
        
        float progress = static_cast<float>(totalDownloaded) / static_cast<float>(totalExpected) * 100.0f;
        if (progress > 100.0f) progress = 100.0f;
        
        size_t downloaded = static_cast<size_t>(totalDownloaded);
        size_t total = static_cast<size_t>(totalExpected);

        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastCallback = std::chrono::duration_cast<std::chrono::milliseconds>(now - data->lastCallbackTime);
        float progressDiff = std::abs(progress - data->lastProgress);

        bool shouldUpdate = (timeSinceLastCallback.count() >= 250) || 
                           (progressDiff >= 1.0f) || 
                           (progress >= 100.0f && data->lastProgress < 100.0f);

        if (!shouldUpdate) {
            return 0;
        }

        data->lastCallbackTime = now;
        data->lastProgress = progress;

        {
            std::lock_guard<std::mutex> lock(data->manager->downloadsMutex);
            auto it2 = data->manager->findDownload(data->downloadId);
            if (it2 != data->manager->downloads.end()) {
                it2->progress = progress;
                it2->downloadedSize = downloaded;
                it2->totalSize = total;
            }
        }
        
        if (data->manager->globalProgressCallback) {
            data->manager->globalProgressCallback(data->downloadId, progress, downloaded, total);
        }
    }
    return 0;
}

// Struttura per catturare gli header durante il download
struct HeaderData {
    size_t contentLength;
    bool contentLengthFound;
    
    HeaderData() : contentLength(0), contentLengthFound(false) {}
};

// Callback per analizzare gli header e catturare Content-Length
static size_t HeaderCallback(char* buffer, size_t size, size_t nitems, void* userdata) {
    HeaderData* data = static_cast<HeaderData*>(userdata);
    size_t totalSize = size * nitems;
    
    std::string header(buffer, totalSize);
    
    // Cerca Content-Length nell'header
    if (header.find("Content-Length:") == 0) {
        size_t colonPos = header.find(':');
        if (colonPos != std::string::npos) {
            std::string lengthStr = header.substr(colonPos + 1);
            // Rimuovi spazi bianchi
            lengthStr.erase(0, lengthStr.find_first_not_of(" \t"));
            lengthStr.erase(lengthStr.find_last_not_of(" \t\r\n") + 1);
            
            try {
                data->contentLength = std::stoull(lengthStr);
                data->contentLengthFound = true;
                brls::Logger::info("HeaderCallback: Found Content-Length: {}", data->contentLength);
            } catch (const std::exception& e) {
                brls::Logger::error("HeaderCallback: Error parsing Content-Length: {}", e.what());
            }
        }
    }
    
    return totalSize;
}

// Struttura per il write callback con progresso
struct WriteProgressData {
    std::FILE* file;
    SimpleProgressData* progressData;
    size_t totalDownloaded;
    size_t totalExpected;
    size_t lastReportedSize;
    HeaderData* headerData; // Puntatore agli header data per aggiornamenti dinamici
};

// Write callback personalizzato che aggiorna anche il progresso
static size_t SimplifiedWriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    WriteProgressData* data = static_cast<WriteProgressData*>(userp);
    if (!data || !data->file || !data->progressData) {
        return 0;
    }
    
    size_t totalSize = size * nmemb;
    size_t written = std::fwrite(contents, 1, totalSize, data->file);
    
    if (written > 0) {
        data->totalDownloaded += written;
        
        // Aggiorna la dimensione totale se abbiamo ricevuto Content-Length dagli header
        if (data->headerData && data->headerData->contentLengthFound && data->totalExpected == 0) {
            data->totalExpected = data->headerData->contentLength;
            brls::Logger::info("SimplifiedWriteCallback: Updated total expected size to {} bytes from headers", data->totalExpected);
        }
        
        // Aggiorna progresso ogni 5MB o se completo per ridurre la frequenza di callback
        if (data->totalDownloaded - data->lastReportedSize > 5242880 || 
            (data->totalExpected > 0 && data->totalDownloaded >= data->totalExpected)) {
            data->lastReportedSize = data->totalDownloaded;
            
            if (data->totalExpected > 0) {
                float progress = static_cast<float>(data->totalDownloaded) / static_cast<float>(data->totalExpected) * 100.0f;
                if (progress > 100.0f) progress = 100.0f;
                if (progress < 0.0f) progress = 0.0f; // Assicurati che non sia negativo
                
                brls::Logger::debug("SimplifiedWriteCallback: {:.1f}% ({}/{} bytes)", 
                                   progress, data->totalDownloaded, data->totalExpected);

                // Aggiorna l'item nel manager
                if (data->progressData->manager) {
                    std::lock_guard<std::mutex> lock(data->progressData->manager->downloadsMutex);
                    auto it = data->progressData->manager->findDownload(data->progressData->downloadId);
                    if (it != data->progressData->manager->downloads.end()) {
                        it->progress = progress;
                        it->downloadedSize = data->totalDownloaded;
                        it->totalSize = data->totalExpected;
                        
                        // Se il download è completato, aggiorna lo status
                        if (progress >= 100.0f && it->status == DownloadStatus::DOWNLOADING) {
                            it->status = DownloadStatus::COMPLETED;
                            brls::Logger::info("SimplifiedWriteCallback: Download {} completed at {:.1f}%", data->progressData->downloadId, progress);
                        }
                    }
                }
                
                // Chiama il callback globale se presente, ma solo se non è già completato
                bool isDownloadCompleted = (progress >= 100.0f);
                if (data->progressData->manager && data->progressData->manager->globalProgressCallback) {
                    // Evita chiamate multiple del callback al completamento
                    bool alreadyCompleted = false;
                    {
                        std::lock_guard<std::mutex> completedLock(data->progressData->manager->completedDownloadsMutex);
                        alreadyCompleted = data->progressData->manager->completedDownloads.count(data->progressData->downloadId) > 0;
                    }
                    
                    if (!isDownloadCompleted || !alreadyCompleted) {
                        brls::Logger::debug("SimplifiedWriteCallback: Calling global progress callback with {:.1f}%", progress);
                        try {
                            data->progressData->manager->globalProgressCallback(data->progressData->downloadId, progress, data->totalDownloaded, data->totalExpected);
                            
                            // Segna come completato se raggiunge il 100%
                            if (isDownloadCompleted) {
                                std::lock_guard<std::mutex> completedLock(data->progressData->manager->completedDownloadsMutex);
                                data->progressData->manager->completedDownloads.insert(data->progressData->downloadId);
                                brls::Logger::info("SimplifiedWriteCallback: Download completed, marked to avoid duplicate callbacks");
                            }
                        } catch (const std::exception& e) {
                            brls::Logger::error("SimplifiedWriteCallback: Exception in global progress callback: {}", e.what());
                        } catch (...) {
                            brls::Logger::error("SimplifiedWriteCallback: Unknown exception in global progress callback");
                        }
                    } else {
                        brls::Logger::debug("SimplifiedWriteCallback: Skipping callback for already completed download {}", data->progressData->downloadId);
                    }
                } else {
                    brls::Logger::debug("SimplifiedWriteCallback: No global progress callback available");
                }
                
                // Se il download è completato, continua a restituire il numero di bytes scritti
                if (isDownloadCompleted) {
                    brls::Logger::info("SimplifiedWriteCallback: Download completed, but continuing to return written bytes");
                    // Non restituire 0 qui, continua con il normal flow per restituire 'written'
                }
            }
        }
    }
    
    return written;
}

// Callback semplificato per il progresso del download (solo per downloadSimplified)
static int SimplifiedProgressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    SimpleProgressData* data = static_cast<SimpleProgressData*>(clientp);
    if (!data || !data->manager) {
        return 0;
    }

    // Controlla se il manager è in fase di shutdown
    if (data->manager->shouldStop.load()) {
        return 1; // Interrompe il download
    }
    
    if (dltotal > 0 && dlnow >= 0 && dlnow <= dltotal) {
        // Calcolo semplice del progresso
        float progress = static_cast<float>(dlnow) / static_cast<float>(dltotal) * 100.0f;
        if (progress > 100.0f) progress = 100.0f;
        
        size_t downloaded = static_cast<size_t>(dlnow);
        size_t total = static_cast<size_t>(dltotal);
        
        brls::Logger::debug("SimplifiedProgressCallback: {:.1f}% ({}/{} bytes)", 
                           progress, downloaded, total);

        // Aggiorna l'item nel manager
        {
            std::lock_guard<std::mutex> lock(data->manager->downloadsMutex);
            auto it2 = data->manager->findDownload(data->downloadId);
            if (it2 != data->manager->downloads.end()) {
                it2->progress = progress;
                it2->downloadedSize = downloaded;
                it2->totalSize = total;
            }
        }
        
        // Chiama il callback globale se presente
        if (data->manager->globalProgressCallback) {
            data->manager->globalProgressCallback(data->downloadId, progress, downloaded, total);
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
    
    // Rimuovi caratteri non validi dal filename e limita la lunghezza
    std::replace_if(filename.begin(), filename.end(), [](char c) {
        // Caratteri non validi per i nomi file su diversi sistemi operativi
        return c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || 
               c == '<' || c == '>' || c == '|' || c == '\0' || c < 32;
    }, '_');
    
    // Limita la lunghezza del nome file per evitare problemi
    if (filename.length() > 200) {
        // Mantieni l'estensione e l'ID
        std::string extension = ".mp4";
        std::string idPart = "_" + id;
        size_t maxTitleLength = 200 - extension.length() - idPart.length();
        std::string titlePart = title.substr(0, maxTitleLength);
        filename = titlePart + idPart + extension;
    }
    
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
        
        // Reset retry count when manually resuming
        {
            std::lock_guard<std::mutex> retryLock(retryCountMutex);
            globalRetryCount.erase(id);
        }
        
        // Avvia un nuovo thread per il download
        downloadThreads.emplace_back(&DownloadManager::downloadWorker, this, id);
        brls::Logger::info("DownloadManager: Resumed download {} (retry count reset)", id);
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
        
        // Clean up retry count
        {
            std::lock_guard<std::mutex> retryLock(retryCountMutex);
            globalRetryCount.erase(id);
        }
        
        // Clean up completed downloads tracking
        {
            std::lock_guard<std::mutex> completedLock(completedDownloadsMutex);
            completedDownloads.erase(id);
        }
        
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
        
        // Clean up retry count
        {
            std::lock_guard<std::mutex> retryLock(retryCountMutex);
            globalRetryCount.erase(id);
        }
        
        // Clean up completed downloads tracking
        {
            std::lock_guard<std::mutex> completedLock(completedDownloadsMutex);
            completedDownloads.erase(id);
        }
        
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
    
    // Check if we should use chunked download (Switch only)
#ifdef __SWITCH__
    {
        std::string url, localPath;
        size_t downloadedSize = 0;
        bool found = false;
        
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end()) {
                url = it->url;
                localPath = it->localPath; 
                downloadedSize = it->downloadedSize;
                found = true;
            }
        }
        
        if (found) {
            // Skip HEAD request to avoid hangs - go directly to simplified download
            brls::Logger::info("DownloadManager: Skipping HEAD request, using simplified download for Switch");
            downloadSimplified(id, url, localPath, downloadedSize);
            return;
        }
    }
#endif
    
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
    
    // Crea la directory di download se non esiste
    try {
        std::string dirPath = getDownloadDirectory();
        brls::Logger::debug("DownloadManager: Checking download directory: {}", dirPath);
        
        // Controlla se la directory esiste già
        struct stat statInfo;
        if (stat(dirPath.c_str(), &statInfo) != 0) {
            // La directory non esiste, tenta di crearla
            brls::Logger::info("DownloadManager: Download directory does not exist, creating: {}", dirPath);
            if (mkdir(dirPath.c_str(), 0755) != 0 && errno != EEXIST) {
                throw std::runtime_error("mkdir failed: " + std::string(strerror(errno)));
            }
            brls::Logger::info("DownloadManager: Created download directory: {}", dirPath);
        } else {
            brls::Logger::debug("DownloadManager: Download directory already exists: {}", dirPath);
        }
    } catch (const std::exception& e) {
        brls::Logger::error("DownloadManager: Failed to create download directory: {}", e.what());
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end()) {
                it->status = DownloadStatus::FAILED;
                it->error = "Failed to create download directory: " + std::string(e.what());
            }
        }
        
        curl_easy_cleanup(curl);
        
        // Chiama callback di errore
        DownloadManager::DownloadErrorCallback errorCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto errorIt = downloadErrorCallbacks.find(id);
            if (errorIt != downloadErrorCallbacks.end()) {
                errorCallback = errorIt->second;
            }
        }
        
        if (errorCallback) {
            errorCallback(id, "Failed to create download directory");
        }
        return;
    }
    
    // Apri il file in modalità appropriata
    brls::Logger::debug("DownloadManager: Attempting to open file: {}", item.localPath);
    
    std::ofstream outFile;
    if (resumeDownload) {
        outFile.open(item.localPath, std::ios::binary | std::ios::app);
        brls::Logger::debug("DownloadManager: Opening file in append mode for resume");
    } else {
        outFile.open(item.localPath, std::ios::binary);
        brls::Logger::debug("DownloadManager: Opening file in write mode");
    }
    
    if (!outFile.is_open()) {
        brls::Logger::error("DownloadManager: Failed to open file: {}", item.localPath);
        brls::Logger::error("DownloadManager: errno: {} ({})", errno, strerror(errno));
        
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end()) {
                it->status = DownloadStatus::FAILED;
                it->error = "Failed to open output file: " + std::string(strerror(errno));
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
    
    // Add buffer size optimizations - different for Switch
#ifdef __SWITCH__
    // Nintendo Switch: very small buffers and ultra-conservative settings for maximum stability
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 32 * 1024); // 32KB buffer (very small for Switch stability)
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 0L); // Enable Nagle algorithm on Switch (may help with small packets)
#else
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 128 * 1024); // 128KB buffer
    curl_easy_setopt(curl, CURLOPT_TCP_NODELAY, 1L); // Disable Nagle algorithm
#endif
    
    // Add keep-alive and connection reuse settings - Switch-specific
#ifdef __SWITCH__
    // Nintendo Switch: very conservative keep-alive settings and no connection reuse
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);  // Send keepalive probe after 30s idle (shorter for stability)
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 15L); // Send probe every 15s (more frequent)
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L); // Disable connection reuse on Switch (may help stability)
#else
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L);
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 30L);  // Send keepalive probe after 30s idle
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 10L); // Send probe every 10s
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 0L); // Allow connection reuse
#endif
    
    // Force HTTP version - Switch may have issues with HTTP/2
#ifdef __SWITCH__
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0); // Use HTTP/1.0 on Switch for maximum compatibility
#else
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1); // Use HTTP/1.1 on other platforms
#endif
    
    // Add connection maintenance settings - Switch-specific
#ifdef __SWITCH__
    // Nintendo Switch: disable aggressive connection management + additional TCP settings
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);  // Force new connections on Switch
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 1L);   // Keep minimal connection cache
    curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 1L);       // Wait for pipeline (more conservative)
    curl_easy_setopt(curl, CURLOPT_MAXAGE_CONN, 15L);  // Keep connections alive for 15 sec only (reduced)
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);       // Don't use signals (important for Switch)
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_ALPN, 0L); // Disable ALPN (may cause issues on Switch)
    curl_easy_setopt(curl, CURLOPT_SSL_ENABLE_NPN, 0L);  // Disable NPN (may cause issues on Switch)
#else
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 0L);  // Allow reusing connections
    curl_easy_setopt(curl, CURLOPT_MAXCONNECTS, 10L);   // Keep connection cache
    curl_easy_setopt(curl, CURLOPT_PIPEWAIT, 0L);       // Don't wait for pipeline
    curl_easy_setopt(curl, CURLOPT_MAXAGE_CONN, 120L);  // Keep connections alive for 2 min
#endif
    
    // Add retry settings for better reliability - be more permissive for Switch
#ifdef __SWITCH__
    // Nintendo Switch has more aggressive network timeouts, be very lenient
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 64L); // 64 bytes/s minimum speed (very conservative for Switch)
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 300L); // Allow 5 minutes of low speed (very patient for Switch)
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 90L); // 90 seconds to connect (very patient for Switch)
#else
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 512L); // 512 bytes/s minimum speed (reduced)
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 120L); // Allow 2 minutes of low speed before timeout (increased)
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L); // 30 seconds to connect
#endif
    // Note: Total timeout will be set later, don't set it here
    
    // Simplified socket options without lambda that might cause issues
    curl_easy_setopt(curl, CURLOPT_SOCKOPTFUNCTION, nullptr);
    
    // Disable Expect: 100-continue header that can cause issues with some servers
    struct curl_slist* expect_header = NULL;
    expect_header = curl_slist_append(expect_header, "Expect:");
    headers = curl_slist_append(headers, "Expect:");
    
    // Add Connection: keep-alive explicitly
    headers = curl_slist_append(headers, "Connection: keep-alive");
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    headers = curl_slist_append(headers, "Pragma: no-cache");
    
    // Set final timeout - no total timeout for large files, let low speed timeout handle stalls
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No total timeout - rely on low speed timeout only
    
    // Additional diagnostics for Switch - check for problematic sources
#ifdef __SWITCH__
    // Check if this is a known problematic CDN/server for Switch
    std::string urlLower = item.url;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    bool isPotentiallyProblematic = false;
    
    if (urlLower.find("github.com") != std::string::npos ||
        urlLower.find("githubusercontent.com") != std::string::npos ||
        urlLower.find("discord") != std::string::npos ||
        urlLower.find("mega.nz") != std::string::npos) {
        isPotentiallyProblematic = true;
        brls::Logger::warning("DownloadManager: Detected potentially problematic source for Switch: {}", item.url);
        brls::Logger::info("DownloadManager: Using extra-conservative settings for this source");
        
        // Even more conservative settings for problematic sources
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 32L); // 32 bytes/s minimum (extremely conservative)
        curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 600L); // Allow 10 minutes of low speed
        curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16384L); // 16KB buffer (even smaller)
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 15L);  // Keep-alive every 15s
        curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 10L); // Probe every 10s
    }
#endif
    
    brls::Logger::info("DownloadManager: Starting download from URL: {}", item.url);
    brls::Logger::info("DownloadManager: Target file: {}", item.localPath);
#ifdef __SWITCH__
    brls::Logger::info("DownloadManager: Using Nintendo Switch ultra-conservative settings");
    brls::Logger::info("DownloadManager: - HTTP/1.0, 32KB buffer, 64 bytes/s min speed, 300s timeout");
    brls::Logger::info("DownloadManager: - Fresh connections, aggressive keep-alive (30s/15s), no connection reuse");
    brls::Logger::info("DownloadManager: - Disabled ALPN/NPN, no signals, 90s connect timeout");
#endif
    
    // Controlla se dobbiamo fermarci prima di iniziare il download
    if (shouldStop.load()) {
        brls::Logger::debug("DownloadManager: Aborting download {} due to shutdown", id);
        if (headers) {
            curl_slist_free_all(headers);
        }
        curl_easy_cleanup(curl);
        return;
    }
    
    brls::Logger::info("DownloadManager: Beginning download session for ID: {}", id);
    brls::Logger::info("DownloadManager: Resume mode: {}, existing size: {} bytes", resumeDownload, existingFileSize);
    
    // Track initial progress to detect if download is actually progressing
    size_t initialDownloadedSize = 0;
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            initialDownloadedSize = it->downloadedSize;
        }
    }
    
    // Perform download in single continuous session
    brls::Logger::info("DownloadManager: Starting CURL perform for {}", id);
    auto startTime = std::chrono::steady_clock::now();
    CURLcode res = curl_easy_perform(curl);
    auto endTime = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::seconds>(endTime - startTime);
    
    outFile.close();
    
    brls::Logger::info("DownloadManager: Download session completed for ID: {}, CURL result: {} ({}), duration: {}s", 
                      id, static_cast<int>(res), curl_easy_strerror(res), duration.count());
    
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
    
    // Ottieni informazioni aggiuntive su cosa è successo
    curl_off_t contentLength = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    
    double totalTime = 0;
    curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
    
    double downloadSpeed = 0;
    curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &downloadSpeed);
    
    brls::Logger::info("DownloadManager: Content-Length: {} bytes, Total time: {:.1f}s, Speed: {:.1f} bytes/s", 
                      contentLength, totalTime, downloadSpeed);
    
    // Aggiungi informazioni specifiche per debugging interruzioni
    if (res == CURLE_PARTIAL_FILE) {
        brls::Logger::warning("DownloadManager: Partial transfer detected - this could be due to:");
        brls::Logger::warning("  - Server closed connection early");
        brls::Logger::warning("  - Network timeout or interruption");  
#ifdef __SWITCH__
        brls::Logger::warning("  - Low speed timeout (limit: 64 bytes/s for 300s on Switch)");
        brls::Logger::warning("  - Nintendo Switch network stack limitations (expected behavior)");
        brls::Logger::warning("  - WiFi power management or interference");
        brls::Logger::warning("  - Switch sleep mode or background app suspension");
#else
        brls::Logger::warning("  - Low speed timeout (limit: 512 bytes/s for 120s)");
#endif
        brls::Logger::warning("  - Server-side rate limiting");
    }
    
    // Check available disk space before starting download
    if (contentLength > 0) {
        std::string downloadDir = getDownloadDirectory();
        
#ifdef __SWITCH__
        // Nintendo Switch: check SD card space
        s64 freeSpace = 0;
        Result rc = nsGetFreeSpaceSize(NcmStorageId_SdCard, &freeSpace);
        if (R_SUCCEEDED(rc)) {
            brls::Logger::info("DownloadManager: Available space on SD: {} bytes, needed: {} bytes", freeSpace, contentLength);
            if (freeSpace < contentLength + (100 * 1024 * 1024)) { // Reserve 100MB
                brls::Logger::error("DownloadManager: Insufficient space - need {} bytes, have {} bytes", contentLength, freeSpace);
                {
                    std::lock_guard<std::mutex> lock(downloadsMutex);
                    auto it = findDownload(id);
                    if (it != downloads.end()) {
                        it->status = DownloadStatus::FAILED;
                        it->error = "Insufficient storage space";
                    }
                }
                if (headers) curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return;
            }
        }
#else
        // Other platforms: use statvfs
        struct statvfs stat;
        if (statvfs(downloadDir.c_str(), &stat) == 0) {
            uint64_t freeSpace = static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
            brls::Logger::info("DownloadManager: Available space: {} bytes, needed: {} bytes", freeSpace, contentLength);
            if (freeSpace < static_cast<uint64_t>(contentLength) + (100 * 1024 * 1024)) { // Reserve 100MB
                brls::Logger::error("DownloadManager: Insufficient space - need {} bytes, have {} bytes", contentLength, freeSpace);
                {
                    std::lock_guard<std::mutex> lock(downloadsMutex);
                    auto it = findDownload(id);
                    if (it != downloads.end()) {
                        it->status = DownloadStatus::FAILED;
                        it->error = "Insufficient storage space";
                    }
                }
                if (headers) curl_slist_free_all(headers);
                curl_easy_cleanup(curl);
                return;
            }
        }
#endif
    }
    
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            // Verifica se il download è andato a buon fine
            bool downloadSuccessful = false;
            
            if (httpCode >= 200 && httpCode < 300) {
                if (res == CURLE_OK) {
                    // Download completed without errors
                    downloadSuccessful = true;
                } else if (res == CURLE_PARTIAL_FILE && contentLength > 0) {
                    // Partial download: check if we should retry or pause
                    float completionRatio = static_cast<float>(totalDownloaded) / static_cast<float>(contentLength);
                    
                    // Check if we made any progress in this session
                    bool madeProgress = (downloadSize > 1024); // At least 1KB downloaded in this session
                    bool madeGoodProgress = (downloadSize > 10 * 1024 * 1024); // At least 10MB in this session
                    
                    // Se abbiamo scaricato una piccola percentuale, potrebbe essere un errore di rete temporaneo
                    // Retry automaticamente per le prime volte, poi metti in pausa
                    int currentRetries = 0;
                    {
                        std::lock_guard<std::mutex> retryLock(retryCountMutex);
                        currentRetries = ++globalRetryCount[id];
                    }
                    
                    // Be more aggressive with retries if we're not making good progress
#ifdef __SWITCH__
                    // Nintendo Switch: many more retries due to frequent network interruptions
                    int maxRetries = madeGoodProgress ? 15 : 10; // Even more retries on Switch since interruptions are expected
#else
                    int maxRetries = madeGoodProgress ? 3 : 2; // Fewer retries if downloads are very small
#endif
                    
                    if (currentRetries <= maxRetries && completionRatio < 0.95f && madeProgress) {
#ifdef __SWITCH__
                        brls::Logger::info("DownloadManager: Switch network interruption at {:.1f}% ({}/{} bytes), auto-resuming #{}/{}", 
                                         completionRatio * 100.0f, totalDownloaded, contentLength, currentRetries, maxRetries);
#else
                        brls::Logger::warning("DownloadManager: Partial download detected at {:.1f}% ({}/{} bytes), attempting retry #{}/{}", 
                                            completionRatio * 100.0f, totalDownloaded, contentLength, currentRetries, maxRetries);
#endif
                        
                        // Aggiorna lo stato e mantieni in downloading
                        it->status = DownloadStatus::DOWNLOADING;
                        it->progress = completionRatio * 100.0f;
                        it->downloadedSize = totalDownloaded;
                        it->totalSize = contentLength;
                        
                        // Clean up current connection
                        if (headers) curl_slist_free_all(headers);
                        curl_easy_cleanup(curl);
                        
                        // Wait progressively much longer between retries - Switch needs more time
#ifdef __SWITCH__
                        // Nintendo Switch: minimal waits for near-instant resume on network interruptions
                        int waitSeconds = std::min(currentRetries, 3); // 1,2,3,3,3... seconds - very fast resume on Switch
                        brls::Logger::info("DownloadManager: Switch auto-resume in {} seconds (network interruption is normal)...", waitSeconds);
#else
                        int waitSeconds = madeGoodProgress ? (currentRetries * 5) : (currentRetries * 10); // Even longer wait for problematic downloads
                        brls::Logger::info("DownloadManager: Waiting {} seconds before retry (strategy: {} progress)...", 
                                         waitSeconds, madeGoodProgress ? "good" : "poor");
#endif
                        
                        // Check for shutdown during wait
                        for (int i = 0; i < waitSeconds && !shouldStop.load(); ++i) {
                            std::this_thread::sleep_for(std::chrono::seconds(1));
                        }
                        
                        // Check again after wait
                        if (shouldStop.load()) {
                            brls::Logger::info("DownloadManager: Shutdown detected, cancelling retry for {}", id);
                            if (headers) curl_slist_free_all(headers);
                            curl_easy_cleanup(curl);
                            return;
                        }
                        
                        // Save current state before retry
                        saveDownloads();
                        
                        // Schedule a new download task instead of recursive call
                        std::thread retryThread([this, id]() {
                            // Double-check that download still exists and is not cancelled
                            {
                                std::lock_guard<std::mutex> lock(downloadsMutex);
                                auto it = findDownload(id);
                                if (it == downloads.end() || shouldStop.load()) {
                                    brls::Logger::info("DownloadManager: Download {} no longer exists or shutdown requested, cancelling retry", id);
                                    return;
                                }
                            }
                            
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            this->downloadWorker(id);
                        });
                        retryThread.detach();
                        return;
                    } else {
                        // Too many retries or close to completion or no progress made, pause for manual resume
                        if (!madeProgress) {
                            brls::Logger::warning("DownloadManager: No progress made in this session, pausing download");
                        }
                        brls::Logger::info("DownloadManager: Partial download paused at {:.1f}% ({}/{} bytes) after {} retries", 
                                         completionRatio * 100.0f, totalDownloaded, contentLength, currentRetries);
                        
                        it->status = DownloadStatus::PAUSED;
                        it->progress = completionRatio * 100.0f;
                        it->downloadedSize = totalDownloaded;
                        it->totalSize = contentLength;
                        it->error = "Download interrupted at " + std::to_string(static_cast<int>(completionRatio * 100.0f)) + "% - can be resumed";
                        saveDownloads();
                        
                        // Reset retry count for this download
                        {
                            std::lock_guard<std::mutex> retryLock(retryCountMutex);
                            globalRetryCount.erase(id);
                        }
                        
                        // Clean up and exit normally
                        if (headers) curl_slist_free_all(headers);
                        curl_easy_cleanup(curl);
                        return;
                    }
                } else if (res == CURLE_PARTIAL_FILE) {
                    // Partial download without known total size: treat as success
                    downloadSuccessful = true;
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
                    
                    // Reset retry count for successful download
                    {
                        std::lock_guard<std::mutex> retryLock(retryCountMutex);
                        globalRetryCount.erase(id);
                    }
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
            
            // Salva informazioni sui chunk per Switch
            item["useChunkedDownload"] = download.useChunkedDownload;
            item["chunkSize"] = download.chunkSize;
            if (download.useChunkedDownload) {
                json chunks = json::array();
                for (const auto& chunk : download.chunks) {
                    json chunkJson;
                    chunkJson["start"] = chunk.start;
                    chunkJson["end"] = chunk.end;
                    chunkJson["completed"] = chunk.completed;
                    chunkJson["tempFilePath"] = chunk.tempFilePath;
                    chunks.push_back(chunkJson);
                }
                item["chunks"] = chunks;
            }
            
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
            
            // Carica informazioni sui chunk per Switch
            if (item.contains("useChunkedDownload")) {
                download.useChunkedDownload = item["useChunkedDownload"];
                if (item.contains("chunkSize")) {
                    download.chunkSize = item["chunkSize"];
                } else {
                    download.chunkSize = 0;
                }
                
                if (download.useChunkedDownload && item.contains("chunks")) {
                    for (const auto& chunkJson : item["chunks"]) {
                        DownloadChunk chunk(chunkJson["start"], chunkJson["end"]);
                        chunk.completed = chunkJson["completed"];
                        chunk.tempFilePath = chunkJson["tempFilePath"];
                        download.chunks.push_back(chunk);
                    }
                    brls::Logger::info("DownloadManager: Loaded {} chunks for chunked download {}", 
                                     download.chunks.size(), download.title);
                }
            }
            
            // Reset status dei download in corso al riavvio
            if (download.status == DownloadStatus::DOWNLOADING) {
                download.status = DownloadStatus::PAUSED;
            }
            
            downloads.push_back(download);
        }
        
        brls::Logger::info("DownloadManager: Loaded {} downloads from state", downloads.size());
        
#ifdef __SWITCH__
        // Nintendo Switch: auto-resume paused downloads after app restart
        autoResumeDownloadsOnSwitch();
#endif
    } catch (const std::exception& e) {
        brls::Logger::error("DownloadManager: Failed to load downloads state: {}", e.what());
    }
}
DownloadManager::DownloadManager() {
    // NON impostare un callback di default - lascia che l'UI imposti i suoi callback
    // globalProgressCallback sarà nullptr inizialmente, il che è corretto
    brls::Logger::info("DownloadManager: Constructor - leaving callbacks unset for UI registration");
    
    // Subscribe to application exit event to stop downloads immediately
    brls::Application::getExitEvent()->subscribe([this]() {
        brls::Logger::debug("DownloadManager: Application exit event received, stopping all downloads");
        shouldStop = true;
        
        // Mark all active downloads as paused to avoid restart attempts
        std::lock_guard<std::mutex> lock(downloadsMutex);
        for (auto& download : downloads) {
            if (download.status == DownloadStatus::DOWNLOADING || 
                download.status == DownloadStatus::PENDING) {
                download.status = DownloadStatus::PAUSED;
                brls::Logger::debug("DownloadManager: Paused download {} due to app exit", download.id);
            }
        }
        
        // Clear all callbacks to prevent any UI updates during shutdown
        globalProgressCallback = nullptr;
        globalCompleteCallback = nullptr;
        downloadProgressCallbacks.clear();
        downloadCompleteCallbacks.clear();
        downloadErrorCallbacks.clear();
    });
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
    if (callback) {
        globalProgressCallback = callback;
        brls::Logger::info("DownloadManager: Global progress callback set (UI callback)");
    } else {
        // Callback di default che fa solo logging sicuro quando l'UI rimuove il suo callback
        globalProgressCallback = [](const std::string& id, float progress, size_t downloaded, size_t total) {
            // Logging minimale per debug senza UI updates
            brls::Logger::debug("DownloadManager: Progress - Download {} at {:.1f}%", id, progress);
        };
        brls::Logger::info("DownloadManager: Global progress callback reset to default (logging only)");
    }
}

void DownloadManager::setGlobalCompleteCallback(GlobalCompleteCallback callback) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    globalCompleteCallback = callback;
    brls::Logger::info("DownloadManager: Global complete callback set");
}

bool DownloadManager::hasGlobalProgressCallback() const {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    // Considera come "non disponibile" solo se il callback è nullptr 
    // (il callback di default conta come disponibile per evitare sovrascrizioni)
    return globalProgressCallback != nullptr;
}

#ifdef __SWITCH__
void DownloadManager::autoResumeDownloadsOnSwitch() {
    // Auto-resume downloads that were paused due to network interruptions
    std::lock_guard<std::mutex> lock(downloadsMutex);
    
    int resumedCount = 0;
    for (auto& download : downloads) {
        if (download.status == DownloadStatus::PAUSED && 
            download.progress > 0.0f && 
            download.progress < 95.0f) { // Only auto-resume partially completed downloads
            
            brls::Logger::info("DownloadManager: Auto-resuming Switch download: {} at {:.1f}%", 
                             download.title, download.progress);
            
            download.status = DownloadStatus::DOWNLOADING;
            
            // Reset retry count for auto-resumed downloads
            {
                std::lock_guard<std::mutex> retryLock(retryCountMutex);
                globalRetryCount.erase(download.id);
            }
            
            // Start download thread
            downloadThreads.emplace_back(&DownloadManager::downloadWorker, this, download.id);
            resumedCount++;
        }
    }
    
    if (resumedCount > 0) {
        brls::Logger::info("DownloadManager: Auto-resumed {} downloads on Switch", resumedCount);
        saveDownloads();
    }
}
#endif

#ifdef __SWITCH__
// Metodi per download a chunk (specifico per Nintendo Switch)

bool DownloadManager::shouldUseChunkedDownload(const std::string& url, size_t fileSize) {
    // Usa download a chunk per file grandi su Switch
    const size_t CHUNK_THRESHOLD = 50 * 1024 * 1024; // 50MB threshold
    
    if (fileSize > CHUNK_THRESHOLD) {
        brls::Logger::info("DownloadManager: File size {} MB exceeds threshold, using chunked download", 
                         fileSize / (1024*1024));
        return true;
    }
    
    // Usa chunk anche per fonti problematiche indipendentemente dalla dimensione
    std::string urlLower = url;
    std::transform(urlLower.begin(), urlLower.end(), urlLower.begin(), ::tolower);
    
    if (urlLower.find("github.com") != std::string::npos ||
        urlLower.find("githubusercontent.com") != std::string::npos ||
        urlLower.find("discord") != std::string::npos ||
        urlLower.find("mega.nz") != std::string::npos) {
        brls::Logger::info("DownloadManager: Problematic source detected, using chunked download for stability");
        return true;
    }
    
    return false;
}

void DownloadManager::setupChunkedDownload(DownloadItem& item, size_t totalSize) {
    const size_t CHUNK_SIZE = 8 * 1024 * 1024; // 8MB chunks per Switch
    
    item.useChunkedDownload = true;
    item.totalSize = totalSize;
    item.chunkSize = CHUNK_SIZE;
    item.chunks.clear();
    
    // Creare chunks
    for (size_t start = 0; start < totalSize; start += CHUNK_SIZE) {
        size_t end = std::min(start + CHUNK_SIZE - 1, totalSize - 1);
        item.chunks.emplace_back(start, end);
        
        // Imposta path temporaneo per chunk
        std::string tempDir = getDownloadDirectory() + "/temp_" + item.id;
        // Crea directory temporanea usando mkdir
        #ifdef __SWITCH__
        mkdir(tempDir.c_str(), 0777);
        #else
        mkdir(tempDir.c_str(), 0755);
        #endif
        item.chunks.back().tempFilePath = tempDir + "/chunk_" + std::to_string(item.chunks.size() - 1);
    }
    
    brls::Logger::info("DownloadManager: Set up {} chunks of ~{} MB each", 
                     item.chunks.size(), CHUNK_SIZE / (1024*1024));
}

void DownloadManager::downloadChunkedFile(const std::string& id) {
    brls::Logger::info("DownloadManager: Starting chunked download for {}", id);
    
    // Download ogni chunk sequenzialmente
    size_t totalChunks = 0;
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it == downloads.end() || !it->useChunkedDownload) {
            brls::Logger::error("DownloadManager: Invalid chunked download setup for {}", id);
            return;
        }
        totalChunks = it->chunks.size();
    }
    
    for (size_t i = 0; i < totalChunks; ++i) {
        if (shouldStop.load()) {
            brls::Logger::info("DownloadManager: Chunked download cancelled for {}", id);
            cleanupChunkFiles(id);
            return;
        }
        
        bool chunkCompleted = false;
        {
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it == downloads.end()) {
                brls::Logger::error("DownloadManager: Download {} disappeared during chunked download", id);
                return;
            }
            chunkCompleted = it->chunks[i].completed;
        }
        
        if (!chunkCompleted) {
            brls::Logger::info("DownloadManager: Downloading chunk {}/{} for {}", 
                             i + 1, totalChunks, id);
            
            // Download chunk senza tenere lock
            downloadSingleChunk(id, i);
            
            // Aggiorna progresso dopo download
            {
                std::lock_guard<std::mutex> lock(downloadsMutex);
                auto it = findDownload(id);
                if (it == downloads.end()) {
                    brls::Logger::error("DownloadManager: Download {} disappeared during chunked download", id);
                    return;
                }
                
                // Conta chunk completati
                size_t completedChunks = 0;
                for (const auto& chunk : it->chunks) {
                    if (chunk.completed) completedChunks++;
                }
                
                it->progress = (static_cast<float>(completedChunks) / it->chunks.size()) * 100.0f;
                it->downloadedSize = completedChunks * it->chunkSize;
                
                brls::Logger::info("DownloadManager: Chunk progress: {}/{} ({:.1f}%)", 
                                 completedChunks, it->chunks.size(), it->progress);
            }
        }
    }
    
    // Assembla il file finale
    if (assembleChunkedFile(id)) {
        brls::Logger::info("DownloadManager: Successfully assembled chunked file for {}", id);
        
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            it->status = DownloadStatus::COMPLETED;
            it->progress = 100.0f;
            it->downloadedSize = it->totalSize;
            
            // Callback completamento
            if (globalCompleteCallback) {
                globalCompleteCallback(id, true);
            }
            
            auto completeCallback = downloadCompleteCallbacks.find(id);
            if (completeCallback != downloadCompleteCallbacks.end()) {
                completeCallback->second(id, it->localPath);
            }
        }
        
        cleanupChunkFiles(id);
    } else {
        brls::Logger::error("DownloadManager: Failed to assemble chunked file for {}", id);
        
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            it->status = DownloadStatus::FAILED;
            it->error = "Failed to assemble chunked file";
            
            if (globalCompleteCallback) {
                globalCompleteCallback(id, false);
            }
        }
        
        cleanupChunkFiles(id);
    }
}

void DownloadManager::downloadSingleChunk(const std::string& id, size_t chunkIndex) {
    DownloadChunk chunk(0, 0);
    std::string url;
    
    // Ottieni info chunk in modo sicuro
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it == downloads.end() || chunkIndex >= it->chunks.size()) {
            brls::Logger::error("DownloadManager: Invalid chunk download request");
            return;
        }
        
        chunk = it->chunks[chunkIndex];
        url = it->url;
    }
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        brls::Logger::error("DownloadManager: Failed to initialize curl for chunk {}", chunkIndex);
        return;
    }
    
    // Apri file temporaneo per chunk
    std::FILE* file = std::fopen(chunk.tempFilePath.c_str(), "wb");
    if (!file) {
        brls::Logger::error("DownloadManager: Failed to open chunk file: {}", chunk.tempFilePath);
        curl_easy_cleanup(curl);
        return;
    }
    
    // Imposta range header per chunk
    std::string range = "bytes=" + std::to_string(chunk.start) + "-" + std::to_string(chunk.end);
    
    // Configurazione curl ultra-conservativa per chunk
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_RANGE, range.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch)");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L); // 2 minuti per chunk
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 32L); // 32 bytes/s
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L); // 1 minuto
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 16384L); // 16KB buffer
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_0);
    curl_easy_setopt(curl, CURLOPT_FRESH_CONNECT, 1L);
    curl_easy_setopt(curl, CURLOPT_FORBID_REUSE, 1L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
    brls::Logger::info("DownloadManager: Starting chunk {} download: {}", chunkIndex, range);
    
    CURLcode res = curl_easy_perform(curl);
    std::fclose(file);
    
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        if (httpCode == 206) { // Partial Content
            std::lock_guard<std::mutex> lock(downloadsMutex);
            auto it = findDownload(id);
            if (it != downloads.end() && chunkIndex < it->chunks.size()) {
                it->chunks[chunkIndex].completed = true;
                brls::Logger::info("DownloadManager: Chunk {} completed successfully", chunkIndex);
            }
        } else {
            brls::Logger::error("DownloadManager: Chunk {} failed with HTTP {}", chunkIndex, httpCode);
            std::remove(chunk.tempFilePath.c_str());
        }
    } else {
        brls::Logger::error("DownloadManager: Chunk {} failed with CURL error {}: {}", 
                           chunkIndex, static_cast<int>(res), curl_easy_strerror(res));
        std::remove(chunk.tempFilePath.c_str());
    }
    
    curl_easy_cleanup(curl);
}

bool DownloadManager::assembleChunkedFile(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it == downloads.end() || !it->useChunkedDownload) {
        return false;
    }
    
    // Verifica che tutti i chunk siano completati
    for (const auto& chunk : it->chunks) {
        if (!chunk.completed) {
            brls::Logger::error("DownloadManager: Cannot assemble file - chunk not completed");
            return false;
        }
    }
    
    brls::Logger::info("DownloadManager: Assembling {} chunks into {}", it->chunks.size(), it->localPath);
    
    // Apri file finale
    std::FILE* finalFile = std::fopen(it->localPath.c_str(), "wb");
    if (!finalFile) {
        brls::Logger::error("DownloadManager: Failed to open final file: {}", it->localPath);
        return false;
    }
    
    // Copia ogni chunk nel file finale
    for (const auto& chunk : it->chunks) {
        std::FILE* chunkFile = std::fopen(chunk.tempFilePath.c_str(), "rb");
        if (!chunkFile) {
            brls::Logger::error("DownloadManager: Failed to open chunk file: {}", chunk.tempFilePath);
            std::fclose(finalFile);
            return false;
        }
        
        char buffer[8192];
        size_t bytesRead;
        while ((bytesRead = std::fread(buffer, 1, sizeof(buffer), chunkFile)) > 0) {
            if (std::fwrite(buffer, 1, bytesRead, finalFile) != bytesRead) {
                brls::Logger::error("DownloadManager: Failed to write to final file");
                std::fclose(chunkFile);
                std::fclose(finalFile);
                return false;
            }
        }
        
        std::fclose(chunkFile);
    }
    
    std::fclose(finalFile);
    brls::Logger::info("DownloadManager: Successfully assembled chunked file");
    return true;
}

void DownloadManager::cleanupChunkFiles(const std::string& id) {
    std::lock_guard<std::mutex> lock(downloadsMutex);
    auto it = findDownload(id);
    if (it == downloads.end() || !it->useChunkedDownload) {
        return;
    }
    
    // Rimuovi file temporanei dei chunk
    for (const auto& chunk : it->chunks) {
        std::remove(chunk.tempFilePath.c_str());
    }
    
    // Rimuovi directory temporanea
    std::string tempDir = getDownloadDirectory() + "/temp_" + id;
    // Rimuovi directory usando rmdir/system
    #ifdef __SWITCH__
    std::string rmCommand = "rm -rf " + tempDir;
    system(rmCommand.c_str());
    #else
    rmdir(tempDir.c_str());
    #endif
    
    brls::Logger::info("DownloadManager: Cleaned up chunk files for {}", id);
}

// Metodo di download semplificato in stile Tinfoil (fallback ultra-stabile)
void DownloadManager::downloadSimplified(const std::string& id, const std::string& url, const std::string& localPath, size_t downloadedSize) {
    brls::Logger::info("DownloadManager: Using simplified download method for {}", id);
    brls::Logger::info("DownloadManager: Step 1 - Got parameters: URL={}, Path={}, Size={}", 
                      url.substr(0, 50) + "...", localPath, downloadedSize);
    
    // Disabilita lo screen dimming durante il download
    brls::Application::getPlatform()->disableScreenDimming(true, "Download in corso", "TsVitch");
    brls::Logger::info("DownloadManager: Screen dimming disabled for download");
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        brls::Logger::error("DownloadManager: Failed to initialize curl");
        // Riabilita lo screen dimming in caso di errore
        brls::Application::getPlatform()->disableScreenDimming(false, "Download failed", "TsVitch");
        return;
    }
    brls::Logger::info("DownloadManager: Step 2 - CURL initialized");
    
    std::FILE* file = std::fopen(localPath.c_str(), "ab");
    if (!file) {
        brls::Logger::error("DownloadManager: Cannot open file: {}", localPath);
        curl_easy_cleanup(curl);
        return;
    }
    brls::Logger::info("DownloadManager: Step 3 - File opened");
    
    // Configurazione più robusta con header che spesso servono
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    
    // NON usare fwrite direttamente, useremo il nostro callback personalizzato
    // curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, fwrite);
    // curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L); // No timeout totale - usa solo low speed timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L); // 30 secondi per connessione
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 60L); // Se velocità è bassa per 60 secondi
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 1024L); // Considera "bassa" sotto 1KB/s
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    
    // Opzioni per migliorare la resilienza delle connessioni
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPALIVE, 1L); // Keep-alive TCP
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPIDLE, 120L); // Inizia keep-alive dopo 2 minuti
    curl_easy_setopt(curl, CURLOPT_TCP_KEEPINTVL, 60L); // Intervallo keep-alive di 1 minuto
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, CURL_MAX_READ_SIZE); // Buffer più grande
    
    // IMPORTANTE: NON configurare il callback di progresso qui
    // Lo configureremo SOLO dopo la HEAD request, prima del download vero
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L); // Disabilita progresso per ora
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    curl_easy_setopt(curl, CURLOPT_AUTOREFERER, 1L);
    curl_easy_setopt(curl, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_1_1);
    
    // User-Agent più realistico
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (Nintendo Switch; WebApplet) AppleWebKit/609.4.0 (KHTML, like Gecko) TsVitch/1.0");
    
    // Header aggiuntivi che spesso servono
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Accept: */*");
    headers = curl_slist_append(headers, "Accept-Language: en-US,en;q=0.9");
    headers = curl_slist_append(headers, "Accept-Encoding: identity");
    headers = curl_slist_append(headers, "Connection: close");
    headers = curl_slist_append(headers, "Cache-Control: no-cache");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // SSL e certificati
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
    
    // Verbose per debug
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1L);
    
    // Prova prima una richiesta HEAD per verificare se il server è raggiungibile
    brls::Logger::info("DownloadManager: Step 4.1 - Testing server connectivity with HEAD request");
    
    curl_easy_setopt(curl, CURLOPT_NOBODY, 1L); // HEAD request
    CURLcode head_res = curl_easy_perform(curl);
    
    long head_http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &head_http_code);
    
    // Cerca di ottenere la dimensione del file dalla HEAD response
    curl_off_t contentLength = 0;
    curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &contentLength);
    size_t expectedSize = 0;
    if (contentLength > 0) {
        expectedSize = static_cast<size_t>(contentLength);
        brls::Logger::info("DownloadManager: Step 4.2.1 - Expected file size from HEAD: {} bytes", expectedSize);
    }
    
    brls::Logger::info("DownloadManager: Step 4.2 - HEAD request result: {}, HTTP: {}", 
                      static_cast<int>(head_res), head_http_code);
    
    if (head_res != CURLE_OK) {
        brls::Logger::error("DownloadManager: HEAD request failed: {}", curl_easy_strerror(head_res));
    }
    
    // Reset per il download vero e proprio
    curl_easy_setopt(curl, CURLOPT_NOBODY, 0L); // Torna a GET
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L); // Assicurati che sia GET
    
    // ORA configura il write callback personalizzato e progresso per il download vero
    // Usa variabili locali invece di static per evitare conflitti tra download
    SimpleProgressData progressData;
    progressData.manager = this;
    progressData.downloadId = id;
    
    // Header callback per catturare la dimensione corretta dalla GET request
    HeaderData headerData;
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, HeaderCallback);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &headerData);
    
    // Configurazione del write callback personalizzato che aggiorna il progresso
    WriteProgressData writeData;
    writeData.file = file;
    writeData.progressData = &progressData;
    writeData.totalDownloaded = 0;
    writeData.totalExpected = expectedSize; // Inizialmente dalla HEAD, poi aggiornato
    writeData.lastReportedSize = 0; // Inizializza il tracking del progresso
    writeData.headerData = &headerData; // Link agli header data per aggiornamenti dinamici
    
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, SimplifiedWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &writeData);
    
    brls::Logger::info("DownloadManager: Step 4.3 - Configured custom write callback with progress tracking");
    
    brls::Logger::info("DownloadManager: Step 4.4 - CURL configured with enhanced options");
    
    // NON usare resume per ora - potrebbe causare problemi
    /*
    if (downloadedSize > 0) {
        curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(downloadedSize));
        brls::Logger::info("DownloadManager: Step 4 - Set resume from {} bytes", downloadedSize);
    }
    */
    
    brls::Logger::info("DownloadManager: Step 5 - Starting download with retry logic (no total timeout, low speed protection, verbose on)");
    
    // Retry loop per gestire connessioni interrotte
    int maxRetries = 3;
    int retryCount = 0;
    CURLcode res = CURLE_OK;
    bool downloadComplete = false;
    long httpCode = 0; // Dichiara httpCode fuori dal loop
    
    while (!downloadComplete && retryCount <= maxRetries) {
        // Controlla dimensione file attuale per resume
        struct stat currentStat;
        size_t currentSize = 0;
        if (stat(localPath.c_str(), &currentStat) == 0) {
            currentSize = currentStat.st_size;
        }
        
        // Riapri file in modalità append per retry
        if (retryCount > 0) {
            std::fclose(file);
            file = std::fopen(localPath.c_str(), "ab"); // append binary
            if (!file) {
                brls::Logger::error("DownloadManager: Failed to reopen file for retry");
                break;
            }
            writeData.file = file; // Aggiorna il puntatore nel writeData
        }
        
        // Configura resume se necessario
        if (currentSize > 0 && retryCount > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, static_cast<curl_off_t>(currentSize));
            brls::Logger::info("DownloadManager: Retry {} - Resuming from {} bytes", retryCount, currentSize);
            
            // Aggiorna i dati di progresso per il resume
            writeData.totalDownloaded = currentSize;
            writeData.lastReportedSize = currentSize;
        } else if (retryCount > 0) {
            curl_easy_setopt(curl, CURLOPT_RESUME_FROM_LARGE, 0L);
            brls::Logger::info("DownloadManager: Retry {} - Starting from beginning", retryCount);
        }
        
        // Ottieni info su quanto scaricheremo
        double downloadedContentLength = 0;
        curl_easy_setopt(curl, CURLOPT_HEADER, 0L); // No headers nel output
        
        res = curl_easy_perform(curl);
        
        // Ottieni statistiche del download
        curl_easy_getinfo(curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &downloadedContentLength);
        double downloadSpeed = 0;
        curl_easy_getinfo(curl, CURLINFO_SPEED_DOWNLOAD, &downloadSpeed);
        double totalTime = 0;
        curl_easy_getinfo(curl, CURLINFO_TOTAL_TIME, &totalTime);
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode); // Usa la variabile già dichiarata
        
        brls::Logger::info("DownloadManager: Step 6.{} - Download result: {}, HTTP: {}, ContentLength: {:.0f}, Speed: {:.0f} B/s, Time: {:.1f}s", 
                          retryCount, static_cast<int>(res), httpCode, downloadedContentLength, downloadSpeed, totalTime);
        
        // Controlla se il download è completo
        if (res == CURLE_OK) {
            downloadComplete = true;
        } else if (res == CURLE_PARTIAL_FILE && retryCount < maxRetries) {
            retryCount++;
            brls::Logger::warning("DownloadManager: Partial file received, retrying ({}/{})", retryCount, maxRetries);
            // Aspetta un po' prima del retry
            std::this_thread::sleep_for(std::chrono::seconds(2));
        } else {
            break; // Errore non recoverable o troppi retry
        }
    }
    
    std::fclose(file);
    
    // Libera header list
    if (headers) {
        curl_slist_free_all(headers);
    }
    
    // Controlla dimensione file scaricato
    struct stat st;
    if (stat(localPath.c_str(), &st) == 0) {
        brls::Logger::info("DownloadManager: Step 7 - File size after download: {} bytes", st.st_size);
    } else {
        brls::Logger::error("DownloadManager: Step 7 - Cannot stat file: {}", localPath);
    }
    
    curl_easy_cleanup(curl);
    
    // Aggiorna stato finale (con nuovo lock)
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            if (res == CURLE_OK && httpCode >= 200 && httpCode < 300) {
                struct stat st;
                if (stat(localPath.c_str(), &st) == 0) {
                    it->downloadedSize = st.st_size;
                    
                    // Imposta totalSize se non era già impostato
                    if (it->totalSize == 0 && contentLength > 0) {
                        it->totalSize = static_cast<size_t>(contentLength);
                    }
                    
                    if (it->totalSize > 0) {
                        it->progress = (static_cast<float>(it->downloadedSize) / it->totalSize) * 100.0f;
                    } else {
                        // Se non conosciamo la dimensione totale, assume completo se abbiamo scaricato qualcosa
                        it->progress = (it->downloadedSize > 0) ? 100.0f : 0.0f;
                    }
                    
                    it->status = DownloadStatus::COMPLETED;
                    it->progress = 100.0f;
                    brls::Logger::info("DownloadManager: Step 8 - Download completed successfully ({} bytes)", it->downloadedSize);
                }
            } else if (res == CURLE_PARTIAL_FILE && httpCode == 200) {
                // Download parziale ma HTTP 200 - questo è normale, implementa retry automatico
                struct stat st;
                if (stat(localPath.c_str(), &st) == 0) {
                    size_t currentSize = st.st_size;
                    it->downloadedSize = currentSize;
                    
                    // Imposta totalSize dalla HEAD request se disponibile
                    if (it->totalSize == 0 && contentLength > 0) {
                        it->totalSize = static_cast<size_t>(contentLength);
                    }
                    
                    if (it->totalSize > 0) {
                        it->progress = (static_cast<float>(it->downloadedSize) / it->totalSize) * 100.0f;
                        float completionRatio = static_cast<float>(it->downloadedSize) / it->totalSize;
                        
                        // Se abbiamo scaricato più del 95%, consideralo completato
                        if (completionRatio >= 0.95f) {
                            it->status = DownloadStatus::COMPLETED;
                            it->progress = 100.0f;
                            brls::Logger::info("DownloadManager: Step 8 - Download 95%+ complete ({:.1f}%), marking as completed", completionRatio * 100.0f);
                        } else {
                            // Metti in pausa per retry automatico
                            it->status = DownloadStatus::PAUSED;
                            brls::Logger::info("DownloadManager: Step 8 - Download paused for retry at {:.1f}% ({}/{} bytes)", 
                                             it->progress, it->downloadedSize, it->totalSize);
                            
                            // Pianifica un retry automatico dopo 3 secondi
                            std::thread retryThread([this, id]() {
                                std::this_thread::sleep_for(std::chrono::seconds(3));
                                if (!shouldStop.load()) {
                                    brls::Logger::info("DownloadManager: Auto-retrying download {}", id);
                                    {
                                        std::lock_guard<std::mutex> retryLock(downloadsMutex);
                                        auto retryIt = findDownload(id);
                                        if (retryIt != downloads.end() && retryIt->status == DownloadStatus::PAUSED) {
                                            retryIt->status = DownloadStatus::DOWNLOADING;
                                        }
                                    }
                                    downloadWorker(id); // Riavvia il download worker
                                }
                            });
                            retryThread.detach();
                        }
                    } else {
                        // Non conosciamo la dimensione totale ma abbiamo scaricato qualcosa
                        if (currentSize > 1024 * 1024) { // Se abbiamo scaricato almeno 1MB
                            it->status = DownloadStatus::PAUSED;
                            it->progress = 50.0f; // Progresso sconosciuto
                            brls::Logger::info("DownloadManager: Step 8 - Download paused (unknown total size, {} bytes downloaded)", currentSize);
                        } else {
                            it->status = DownloadStatus::FAILED;
                            it->error = "Download failed: Very small partial transfer";
                            brls::Logger::error("DownloadManager: Step 8 - Download failed: Very small partial transfer");
                        }
                    }
                }
            } else {
                it->status = DownloadStatus::FAILED;
                it->error = "Download failed: " + std::string(curl_easy_strerror(res)) + " (HTTP " + std::to_string(httpCode) + ")";
                brls::Logger::error("DownloadManager: Step 8 - Download failed: {} (HTTP {})", curl_easy_strerror(res), httpCode);
            }
        }
        
        // IMPORTANTE: Salva lo stato aggiornato nel file JSON
        saveDownloads();
        brls::Logger::info("DownloadManager: Saved download state to JSON file");
    }
    
    // Riabilita lo screen dimming alla fine del download
    brls::Application::getPlatform()->disableScreenDimming(false, "Download completato", "TsVitch");
    brls::Logger::info("DownloadManager: Screen dimming re-enabled");
    
    // Chiama i callback appropriati fuori dal lock per evitare deadlock
    bool isCompleted = false;
    bool isFailed = false;
    bool isPaused = false;
    std::string localPathCopy;
    std::string errorMessage;
    
    {
        std::lock_guard<std::mutex> lock(downloadsMutex);
        auto it = findDownload(id);
        if (it != downloads.end()) {
            isCompleted = (it->status == DownloadStatus::COMPLETED);
            isFailed = (it->status == DownloadStatus::FAILED);
            isPaused = (it->status == DownloadStatus::PAUSED);
            localPathCopy = it->localPath;
            errorMessage = it->error;
        }
    }
    
    // Chiama i callback appropriati
    if (isCompleted) {
        brls::Logger::info("DownloadManager: Calling completion callbacks for {}", id);
        
        // Callback di completamento specifico
        DownloadManager::DownloadCompleteCallback completeCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto completeIt = downloadCompleteCallbacks.find(id);
            if (completeIt != downloadCompleteCallbacks.end()) {
                completeCallback = completeIt->second;
            }
        }
        
        if (completeCallback) {
            try {
                completeCallback(id, localPathCopy);
                brls::Logger::debug("DownloadManager: Called download complete callback");
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in complete callback: {}", e.what());
            }
        }
        
        // Callback globale di completamento
        if (globalCompleteCallback) {
            try {
                globalCompleteCallback(id, true);
                brls::Logger::debug("DownloadManager: Called global complete callback");
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in global complete callback: {}", e.what());
            }
        }
    } else if (isFailed) {
        brls::Logger::warning("DownloadManager: Calling error callbacks for {}", id);
        
        // Callback di errore specifico
        DownloadManager::DownloadErrorCallback errorCallback = nullptr;
        {
            std::lock_guard<std::mutex> callbackLock(downloadsMutex);
            auto errorIt = downloadErrorCallbacks.find(id);
            if (errorIt != downloadErrorCallbacks.end()) {
                errorCallback = errorIt->second;
            }
        }
        
        if (errorCallback) {
            try {
                errorCallback(id, errorMessage);
                brls::Logger::debug("DownloadManager: Called download error callback");
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in error callback: {}", e.what());
            }
        }
        
        // Callback globale di completamento (anche per errori)
        if (globalCompleteCallback) {
            try {
                globalCompleteCallback(id, false);
                brls::Logger::debug("DownloadManager: Called global complete callback for error");
            } catch (const std::exception& e) {
                brls::Logger::error("DownloadManager: Exception in global complete callback: {}", e.what());
            }
        }
    } else if (isPaused) {
        brls::Logger::info("DownloadManager: Download {} paused for auto-retry, not calling error callbacks", id);
        // Non chiamare callback di errore per pause temporanee
    }
    
    // Pulisci i callback per questo download SOLO se completato o fallito (non per pause)
    if (isCompleted || isFailed) {
        std::lock_guard<std::mutex> callbackLock(downloadsMutex);
        downloadProgressCallbacks.erase(id);
        downloadCompleteCallbacks.erase(id);
        downloadErrorCallbacks.erase(id);
        brls::Logger::debug("DownloadManager: Cleaned up callbacks for download {}", id);
    }
    
    brls::Logger::info("DownloadManager: Step 9 - Simplified download method finished");
}
#endif