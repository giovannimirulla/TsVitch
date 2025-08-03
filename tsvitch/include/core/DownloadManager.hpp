#pragma once

#include <string>
#include <vector>
#include <functional>
#include <memory>
#include <atomic>
#include <thread>
#include <mutex>
#include <unordered_map>
#include <borealis/core/singleton.hpp>
#include <borealis/core/event.hpp>

enum class DownloadStatus {
    PENDING,
    DOWNLOADING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

// Struttura per tracciare i chunk dei download (specifico per Switch)
struct DownloadChunk {
    size_t start;
    size_t end;
    bool completed;
    std::string tempFilePath;
    
    DownloadChunk(size_t s, size_t e) : start(s), end(e), completed(false) {}
};

struct DownloadItem {
    std::string id;
    std::string title;
    std::string url;
    std::string localPath;
    std::string imageUrl;  // URL dell'immagine del film
    DownloadStatus status;
    float progress;
    size_t totalSize;
    size_t downloadedSize;
    std::string error;
    
    // Supporto per download a chunk (Switch)
    bool useChunkedDownload;
    std::vector<DownloadChunk> chunks;
    size_t chunkSize;
    
    DownloadItem() : status(DownloadStatus::PENDING), progress(0.0f), 
                     totalSize(0), downloadedSize(0), useChunkedDownload(false), chunkSize(0) {}
};

class DownloadManager : public brls::Singleton<DownloadManager> {
public:
    using DownloadProgressCallback = std::function<void(const std::string& id, float progress, size_t downloaded, size_t total)>;
    using DownloadCompleteCallback = std::function<void(const std::string& id, const std::string& filePath)>;
    using DownloadErrorCallback = std::function<void(const std::string& id, const std::string& error)>;
    using GlobalProgressCallback = std::function<void(const std::string& id, float progress, size_t downloaded, size_t total)>;
    using GlobalCompleteCallback = std::function<void(const std::string& id, bool success)>;
    
    // Costruttore e distruttore
    DownloadManager();
    ~DownloadManager();
    
    // Avvia un download (versione semplice senza callback)
    std::string startDownload(const std::string& title, const std::string& url);
    
    // Avvia un download con immagine
    std::string startDownload(const std::string& title, const std::string& url, const std::string& imageUrl);
    
    // Avvia un download con callback
    std::string startDownload(const std::string& title, const std::string& url,
                             DownloadProgressCallback progressCallback,
                             DownloadCompleteCallback completeCallback,
                             DownloadErrorCallback errorCallback);
                             
    // Avvia un download con callback e immagine
    std::string startDownload(const std::string& title, const std::string& url, const std::string& imageUrl,
                             DownloadProgressCallback progressCallback,
                             DownloadCompleteCallback completeCallback,
                             DownloadErrorCallback errorCallback);
    
    // Pausa un download
    void pauseDownload(const std::string& id);
    
    // Riprende un download
    void resumeDownload(const std::string& id);
    
    // Cancella un download
    void cancelDownload(const std::string& id);
    
    // Elimina un download completato
    void deleteDownload(const std::string& id);
    
    // Ottiene tutti i download
    std::vector<DownloadItem> getAllDownloads() const;
    
    // Ottiene un download specifico
    DownloadItem getDownload(const std::string& id) const;
    
    // Salva/carica lo stato dei download
    void saveDownloads();
    void loadDownloads();
    
    // Ottiene la directory dei download
    std::string getDownloadDirectory() const;
    
    // Callback globali per tutti i download
    void setGlobalProgressCallback(GlobalProgressCallback callback);
    void setGlobalCompleteCallback(GlobalCompleteCallback callback);

    // Membri pubblici per il callback di progresso
    std::vector<DownloadItem> downloads;
    mutable std::mutex downloadsMutex;
    
    // Trova un download per ID (versione pubblica per il callback)
    std::vector<DownloadItem>::iterator findDownload(const std::string& id);

public:
    // Callback per-download
    std::unordered_map<std::string, DownloadProgressCallback> downloadProgressCallbacks;
    std::unordered_map<std::string, DownloadCompleteCallback> downloadCompleteCallbacks;
    std::unordered_map<std::string, DownloadErrorCallback> downloadErrorCallbacks;
    
    // Callback globali (devono essere pubblici per il callback statico)
    GlobalProgressCallback globalProgressCallback;
    GlobalCompleteCallback globalCompleteCallback;
    
    // Flag di shutdown accessibile dai callback
    std::atomic<bool> shouldStop{false};
    
private:
    
    std::vector<std::thread> downloadThreads;
    
    // Thread worker per il download
    void downloadWorker(const std::string& id);
    
#ifdef __SWITCH__
    // Metodi per download a chunk (Switch)
    bool shouldUseChunkedDownload(const std::string& url, size_t fileSize);
    void setupChunkedDownload(DownloadItem& item, size_t totalSize);
    void downloadChunkedFile(const std::string& id);
    void downloadSingleChunk(const std::string& id, size_t chunkIndex);
    bool assembleChunkedFile(const std::string& id);
    void cleanupChunkFiles(const std::string& id);
    
    // Metodo di download semplificato in stile Tinfoil (fallback)
    void downloadSimplified(const std::string& id, const std::string& url, const std::string& localPath, size_t downloadedSize);
#endif
    
    // Genera un ID unico per il download
    std::string generateDownloadId() const;
    
    // Trova un download per ID (versione const)
    std::vector<DownloadItem>::const_iterator findDownload(const std::string& id) const;
    
    // Ottiene il path del file di stato
    std::string getDownloadsStatePath() const;
    
#ifdef __SWITCH__
    // Auto-riprende i download su Switch dopo il riavvio
    void autoResumeDownloadsOnSwitch();
#endif
};
