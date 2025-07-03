#include "fragment/home_downloads.hpp"
#include "view/recycling_grid.hpp"
#include "view/download_item_cell.hpp"
#include "core/DownloadManager.hpp"
#include "utils/activity_helper.hpp"
#include <borealis/core/i18n.hpp>
#include <borealis/core/thread.hpp>
#include <borealis/core/application.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/button.hpp>
#include <borealis/views/dialog.hpp>
#include <fstream>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include <functional> // per std::hash

using namespace brls::literals;

// Data source personalizzata per i download
class DownloadDataSource : public RecyclingGridDataSource {
private:
    std::vector<DownloadItem> downloads;
    HomeDownloads* parent;
    bool hasChanges = false;
    std::chrono::steady_clock::time_point lastUpdateTime;
    size_t lastUpdateHash = 0; // Hash degli ultimi dati per evitare confronti inutili

    // Calcola un hash semplice dei download per confronti rapidi
    size_t calculateDownloadsHash(const std::vector<DownloadItem>& items) const {
        size_t hash = 0;
        for (const auto& item : items) {
            hash ^= std::hash<std::string>{}(item.id) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<int>{}(static_cast<int>(item.status)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
            hash ^= std::hash<size_t>{}(static_cast<size_t>(item.progress * 100)) + 0x9e3779b9 + (hash << 6) + (hash >> 2);
        }
        return hash;
    }

public:
    DownloadDataSource(HomeDownloads* parent) : parent(parent), lastUpdateTime(std::chrono::steady_clock::now()) {}

    void updateDownloads(const std::vector<DownloadItem>& newDownloads) {
        auto now = std::chrono::steady_clock::now();
        auto timeSinceLastUpdate = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdateTime);
        
        // Evita aggiornamenti troppo frequenti (meno di 50ms) 
        if (timeSinceLastUpdate.count() < 50) {
            hasChanges = false;
            return;
        }
        
        // Controllo rapido con hash per evitare confronti costosi
        size_t newHash = calculateDownloadsHash(newDownloads);
        if (newHash == lastUpdateHash && downloads.size() == newDownloads.size()) {
            hasChanges = false;
            return;
        }
        
        // Controlla se ci sono effettivamente delle modifiche significative
        hasChanges = false;
        
        if (downloads.size() != newDownloads.size()) {
            hasChanges = true;
            brls::Logger::debug("DownloadDataSource: Size changed {} -> {}", downloads.size(), newDownloads.size());
        } else {
            // Confronta ogni download per vedere se è cambiato significativamente
            for (size_t i = 0; i < downloads.size(); i++) {
                const auto& old = downloads[i];
                const auto& new_ = newDownloads[i];
                
                // Solo cambiamenti significativi causano aggiornamenti UI
                if (old.id != new_.id || 
                    old.status != new_.status || 
                    std::abs(old.progress - new_.progress) > 0.5f || // Ridotto a 0.5% per vedere più aggiornamenti
                    old.title != new_.title ||
                    old.error != new_.error ||
                    std::abs(static_cast<long long>(old.downloadedSize) - static_cast<long long>(new_.downloadedSize)) > 1024*1024) { // O se scaricati più di 1MB
                    
                    // Logga cambiamenti di progresso significativi per debug
                    if (std::abs(old.progress - new_.progress) > 0.5f) {
                        brls::Logger::debug("DownloadDataSource: Download {} progress change - {:.1f}% -> {:.1f}%", 
                            new_.title, old.progress, new_.progress);
                    }
                    
                    // Solo logga cambiamenti di stato importanti
                    if (old.status != new_.status || old.id != new_.id) {
                        brls::Logger::debug("DownloadDataSource: Download {} significant change - status: {} -> {}", 
                            new_.title, static_cast<int>(old.status), static_cast<int>(new_.status));
                    }
                    
                    hasChanges = true;
                    break;
                }
            }
        }
        
        if (hasChanges) {
            downloads = newDownloads;
            lastUpdateHash = newHash;
            lastUpdateTime = now;
            brls::Logger::debug("DownloadDataSource::updateDownloads() updated {} downloads with changes", downloads.size());
        }
    }
    
    bool getHasChanges() const { return hasChanges; }
    
    size_t getListSize() const { return downloads.size(); }
    
    DownloadItem getItem(size_t index) const {
        if (index < downloads.size()) {
            return downloads[index];
        }
        return DownloadItem{}; // Ritorna un item vuoto se l'indice non è valido
    }
    
    void forceRefresh() {
        hasChanges = true; // Forza il prossimo refresh
    }

    size_t getItemCount() override {
        // Aggiungi logging temporaneo per debug
        brls::Logger::debug("DownloadDataSource::getItemCount() called - returning {}", downloads.size());
        return downloads.size();
    }

    RecyclingGridItem* cellForRow(RecyclingGrid* recycler, size_t index) override {
        brls::Logger::info("DownloadDataSource::cellForRow called for index {}", index);
        
        // Verifica che recycler sia valido
        if (!recycler) {
            brls::Logger::error("DownloadDataSource::cellForRow - recycler is null!");
            return nullptr;
        }
        
        // Verifica che l'indice sia valido
        if (index >= downloads.size()) {
            brls::Logger::error("DownloadDataSource::cellForRow index {} out of bounds (size: {})", index, downloads.size());
            return nullptr;
        }
        
        DownloadItemCell* cell = (DownloadItemCell*)recycler->dequeueReusableCell("DownloadItemCell");
        brls::Logger::info("DownloadDataSource::cellForRow dequeued cell: {}", (void*)cell);
        
        if (!cell) {
            brls::Logger::error("DownloadDataSource::cellForRow - cell is null!");
            return nullptr;
        }
        
        try {
            brls::Logger::info("DownloadDataSource::cellForRow setting download item: {}", downloads[index].title);
            brls::Logger::info("DownloadDataSource::cellForRow about to call setDownloadItem...");
            cell->setDownloadItem(downloads[index]);
            brls::Logger::info("DownloadDataSource::cellForRow setDownloadItem completed");
        } catch (const std::exception& e) {
            brls::Logger::error("DownloadDataSource::cellForRow exception in setDownloadItem: {}", e.what());
            return nullptr;
        }
        
        return cell;
    }

    float heightForRow(RecyclingGrid* recycler, size_t index) override {
        return 130.0f; // Aumentata l'altezza per accommodate il nuovo layout con immagini
    }

    void onItemSelected(RecyclingGrid* recycler, size_t index) override {
        if (index < downloads.size()) {
            parent->onDownloadItemSelected(downloads[index]);
        }
    }

    void clearData() override {
        downloads.clear();
    }
};

HomeDownloads::HomeDownloads() {
    brls::Logger::info("HomeDownloads: Constructor started");
    
    this->inflateFromXMLRes("xml/fragment/home_downloads.xml");
    brls::Logger::debug("HomeDownloads: XML inflated");
    
    // Sottoscrivi l'exitEvent per shutdown rapido
    brls::Application::getExitEvent()->subscribe([this]() {
        brls::Logger::debug("HomeDownloads: Application exit event received");

        shouldAutoRefresh = false;
        
        // Notifica immediatamente il thread di refresh
        refreshCondition.notify_all();
    });
    brls::Logger::debug("HomeDownloads: Exit event subscribed");
    
    // Configura la data source
    dataSource = new DownloadDataSource(this);
    brls::Logger::debug("HomeDownloads: Data source created");
    
    // Configura la recycling grid
    setupRecyclingGrid();
    brls::Logger::debug("HomeDownloads: Recycling grid setup completed");
    
    // Carica i download esistenti SOLO se non in shutdown e senza fare chiamate di rete
    brls::Logger::info("HomeDownloads: About to load existing downloads");

    
    // Controlla quanti download sono stati caricati - DISABILITATO PER TESTING
    // auto loadedDownloads = DownloadManager::instance().getAllDownloads();
    // brls::Logger::info("HomeDownloads: Loaded {} downloads from storage", loadedDownloads.size());
    brls::Logger::warning("HomeDownloads: Download loading and getAllDownloads() DISABLED for testing");
    

    
    brls::Logger::info("HomeDownloads: About to call refresh() - DISABLED FOR TESTING");
    // if (!isShuttingDown.load()) {
    //     try {
    //         refresh();
    //         brls::Logger::debug("HomeDownloads: Initial refresh completed");
    //     } catch (const std::exception& e) {
    //         brls::Logger::error("HomeDownloads: Error in initial refresh: {}", e.what());
    //     }
    // }
    brls::Logger::warning("HomeDownloads: Initial refresh() DISABLED for testing");
    
    // Avvia l'aggiornamento automatico SOLO se non ci stiamo spegnendo - DISABILITATO PER TESTING
    // if (!isShuttingDown.load()) {
    //     startAutoRefresh();
    //     brls::Logger::debug("HomeDownloads: Auto-refresh started");
    // }
    brls::Logger::warning("HomeDownloads: Auto-refresh START DISABLED for testing");
    
    // Forza un refresh ritardato per assicurarsi che la UI sia pronta - DISABILITATO PER TESTING
    // Ma SOLO se non ci stiamo spegnendo
    // if (!isShuttingDown.load()) {
    //     std::thread([this]() {
    //         std::this_thread::sleep_for(std::chrono::milliseconds(500)); // Aspetta mezzo secondo
    //         
    //         // Controlla se l'app si sta spegnendo prima di fare sync
    //         if (!isShuttingDown.load() && this->shouldAutoRefresh.load()) {
    brls::Logger::warning("HomeDownloads: Delayed refresh thread DISABLED for testing");
    //             try {
    //                 brls::sync([this]() {
    //                     if (!isShuttingDown.load() && this->shouldAutoRefresh.load()) {
    //                         brls::Logger::info("HomeDownloads: Executing delayed refresh to show loaded downloads");
    //                         if (dataSource && recyclingGrid) {
    //                             auto downloads = DownloadManager::instance().getAllDownloads();
    //                             brls::Logger::info("HomeDownloads: Delayed refresh found {} downloads", downloads.size());
    //                             
    //                             if (!downloads.empty()) {
    //                                 dataSource->updateDownloads(downloads);
    //                                 dataSource->forceRefresh(); // Forza il refresh indipendentemente dai cambiamenti
    //                                 recyclingGrid->reloadData();
    //                                 brls::Logger::info("HomeDownloads: Delayed refresh completed with {} downloads", downloads.size());
    //                             } else {
    //                                 brls::Logger::info("HomeDownloads: No downloads to show in delayed refresh");
    //                             }
    //                         } else {
    //                             brls::Logger::error("HomeDownloads: dataSource or recyclingGrid is null in delayed refresh!");
    //                         }
    //                     }
    //                 });
    //             } catch (const std::exception& e) {
    //                 brls::Logger::error("HomeDownloads: Error in delayed refresh: {}", e.what());
    //             }
    //         }
    //     }).detach(); // Detach il thread per non dover gestirlo
    // }
    
    brls::Logger::info("HomeDownloads: Constructor completed successfully");
}

HomeDownloads::~HomeDownloads() {
    brls::Logger::info("HomeDownloads: Starting destructor");
    
    // Imposta immediatamente i flag di shutdown
    shouldAutoRefresh = false;
    
    brls::Logger::debug("HomeDownloads: Shutdown flags set, stopping auto-refresh");
    
    // Ferma il refresh thread prima di tutto con timeout
    try {
        stopAutoRefresh();
        brls::Logger::debug("HomeDownloads: Auto-refresh stopped");
    } catch (const std::exception& e) {
        brls::Logger::error("HomeDownloads: Error stopping auto-refresh: {}", e.what());
    }
    
    // Rimuovi i callback globali per evitare crash
    try {
        DownloadManager::instance().setGlobalProgressCallback(nullptr);
        DownloadManager::instance().setGlobalCompleteCallback(nullptr);
        brls::Logger::debug("HomeDownloads: Callbacks cleared");
    } catch (const std::exception& e) {
        brls::Logger::error("HomeDownloads: Error clearing callbacks: {}", e.what());
    }
    
    // Pulisci la data source
    if (dataSource) {
        try {
            delete dataSource;
            dataSource = nullptr;
            brls::Logger::debug("HomeDownloads: Data source deleted");
        } catch (const std::exception& e) {
            brls::Logger::error("HomeDownloads: Error deleting data source: {}", e.what());
        }
    }
    
    brls::Logger::info("HomeDownloads: destructor completed");
}

void HomeDownloads::draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) {
    brls::Box::draw(vg, x, y, width, height, style, ctx);
}

void HomeDownloads::onFocusGained() {
    brls::Logger::info("HomeDownloads::onFocusGained() called");
    brls::Box::onFocusGained();
    startAutoRefresh();
    
    // Forza un refresh completo quando la vista ottiene il focus
    brls::Logger::info("HomeDownloads::onFocusGained() - forcing refresh");
    auto downloads = DownloadManager::instance().getAllDownloads();
    brls::Logger::info("HomeDownloads::onFocusGained() - {} downloads available", downloads.size());
    
    if (dataSource && recyclingGrid) {
        dataSource->updateDownloads(downloads);
        dataSource->forceRefresh(); // Forza sempre il refresh
        recyclingGrid->reloadData();
        brls::Logger::info("HomeDownloads::onFocusGained() - forced reload completed");
    } else {
        brls::Logger::error("HomeDownloads::onFocusGained() - dataSource or recyclingGrid is null!");
    }
}

void HomeDownloads::onFocusLost() {
    brls::Box::onFocusLost();
    stopAutoRefresh();
}

void HomeDownloads::startAutoRefresh() {
    if (refreshThreadRunning.load()) {
        brls::Logger::debug("HomeDownloads: Auto-refresh thread already running");
        return;
    }
    
    shouldAutoRefresh = true;
    refreshThreadRunning = true;
    
    brls::Logger::info("HomeDownloads: Starting auto-refresh thread");
    
    refreshThread = std::thread(&HomeDownloads::refreshWorker, this);
}

void HomeDownloads::stopAutoRefresh() {
    shouldAutoRefresh = false;
    
    if (refreshThreadRunning.load()) {
        refreshThreadRunning = false;
        
        // Notifica il thread per svegliarlo dalla condition variable
        refreshCondition.notify_all();
        
        if (refreshThread.joinable()) {
            // Prova il join con timeout per evitare hang
            auto startTime = std::chrono::steady_clock::now();
            bool joined = false;
            
            while (!joined && 
                   std::chrono::duration_cast<std::chrono::milliseconds>(
                       std::chrono::steady_clock::now() - startTime).count() < 1000) {
                refreshCondition.notify_all();
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
                // Non c'è un join con timeout standard, quindi simuliamolo
                if (!refreshThreadRunning.load()) {
                    refreshThread.join();
                    joined = true;
                    break;
                }
            }
            
            if (!joined) {
                brls::Logger::warning("HomeDownloads: Thread join timeout, detaching");
                refreshThread.detach();
            }
        }
        
        brls::Logger::debug("HomeDownloads: Stopped auto-refresh thread");
    }
}

void HomeDownloads::refreshWorker() {
    brls::Logger::info("HomeDownloads: Auto-refresh thread started");
    
    while (refreshThreadRunning.load() && shouldAutoRefresh.load()) {
        try {
            // Controllo aggiuntivo per uscita veloce
            if (!refreshThreadRunning.load() || !shouldAutoRefresh.load()) {
                break;
            }
            
            // Controlla se ci sono download attivi
            auto downloads = DownloadManager::instance().getAllDownloads();
            bool hasActiveDownloads = false;
            
            for (const auto& download : downloads) {
                if (download.status == DownloadStatus::DOWNLOADING || 
                    download.status == DownloadStatus::PENDING) {
                    hasActiveDownloads = true;
                    break; // Non loggare ogni download per performance
                }
            }
        
            if (hasActiveDownloads && shouldAutoRefresh.load()) {
                // Usa sync per eseguire refresh sul thread UI principale
                brls::sync([this]() {
                    if (shouldAutoRefresh.load() && dataSource && recyclingGrid) {
                        refresh();
                    }
                });
                
                // Aspetta 750ms per download attivi ma controlla più frequentemente se deve uscire
                std::unique_lock<std::mutex> lock(refreshMutex);
                refreshCondition.wait_for(lock, std::chrono::milliseconds(750), [this] {
                    return !refreshThreadRunning.load() || !shouldAutoRefresh.load();
                });
            } else {
                // Aspetta meno tempo se non ci sono download attivi per uscire più velocemente
                std::unique_lock<std::mutex> lock(refreshMutex);
                refreshCondition.wait_for(lock, std::chrono::milliseconds(2000), [this] {
                    return !refreshThreadRunning.load() || !shouldAutoRefresh.load();
                });
            }
        } catch (const std::exception& e) {
            brls::Logger::error("HomeDownloads::refreshWorker() exception: {}", e.what());
            // In caso di errore, aspetta prima di riprovare
            std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        }
    }
    
    brls::Logger::info("HomeDownloads: Auto-refresh thread ended");
}

void HomeDownloads::setupRecyclingGrid() {
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() called");
    
    // Verifica che recyclingGrid sia valido
    if (!this->recyclingGrid) {
        brls::Logger::error("HomeDownloads::setupRecyclingGrid() - recyclingGrid is null!");
        return;
    }
    
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() - recyclingGrid is valid: {}", (void*)this->recyclingGrid);
    
    // Registra la cella personalizzata
    this->recyclingGrid->registerCell("DownloadItemCell", DownloadItemCell::create);
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() registered DownloadItemCell");
    
    // Verifica che dataSource sia valido
    if (!dataSource) {
        brls::Logger::error("HomeDownloads::setupRecyclingGrid() - dataSource is null!");
        return;
    }
    
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() - dataSource is valid: {}", (void*)dataSource);
    
    // Imposta la data source
    this->recyclingGrid->setDataSource(dataSource);
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() set data source: {}", (void*)dataSource);
    brls::Logger::debug("HomeDownloads::setupRecyclingGrid() completed successfully");
}

void HomeDownloads::refresh() {
    // Controlla se l'oggetto è ancora valido
    if (!dataSource || !recyclingGrid) {
        brls::Logger::warning("HomeDownloads::refresh() - invalid state, skipping refresh");
        return;
    }
    
    try {
        auto downloads = DownloadManager::instance().getAllDownloads();
        brls::Logger::debug("HomeDownloads::refresh() - got {} downloads from manager", downloads.size());
        
        // Aggiorna la data source (che controllerà se ci sono modifiche)
        dataSource->updateDownloads(downloads);
        
        // Notifica la recycling grid SOLO se ci sono effettivamente modifiche
        if (dataSource->getHasChanges()) {
            brls::Logger::debug("HomeDownloads: About to call notifyDataChanged for {} items", downloads.size());
            this->recyclingGrid->notifyDataChanged();
            brls::Logger::debug("HomeDownloads: Called notifyDataChanged successfully - {} items changed", downloads.size());
            
            // Prova anche a forzare un reloadData se notifyDataChanged non funziona
            this->recyclingGrid->reloadData();
            brls::Logger::debug("HomeDownloads: Also called reloadData()");
        } else {
            // Non fare nulla se non ci sono modifiche per ridurre le chiamate a getItemCount()
            brls::Logger::debug("HomeDownloads: No changes detected, skipping UI update");
        }
    } catch (const std::exception& e) {
        brls::Logger::error("HomeDownloads::refresh() exception: {}", e.what());
    }
}

void HomeDownloads::forceRefresh() {
    // Controlla se l'oggetto è ancora valido
    if (!dataSource || !recyclingGrid || !shouldAutoRefresh.load()) {
        brls::Logger::warning("HomeDownloads::forceRefresh() - invalid state, skipping");
        return;
    }
    
    // Sveglia il thread di refresh per un aggiornamento immediato
    refreshCondition.notify_all();
    
    // Esegue anche un refresh immediato
    refresh();
    
    brls::Logger::debug("HomeDownloads: Force refresh triggered");
}

void HomeDownloads::onDownloadProgress(const std::string& id, float progress) {
    // Usa sync per eseguire l'aggiornamento sul thread UI principale
    // Fai un refresh throttled per evitare troppi aggiornamenti
    static std::chrono::steady_clock::time_point lastRefreshTime;
    static std::mutex refreshMutex;
    
    brls::sync([this, id, progress]() {
        if (shouldAutoRefresh.load()) {
            std::lock_guard<std::mutex> lock(refreshMutex);
            auto now = std::chrono::steady_clock::now();
            auto timeSinceLastRefresh = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastRefreshTime);
            
            // Aggiorna più frequentemente (200ms) per vedere i progressi
            if (timeSinceLastRefresh.count() >= 200) {
                brls::Logger::debug("HomeDownloads::onDownloadProgress - refreshing for download {} at {:.1f}%", id, progress);
                refresh();
                lastRefreshTime = now;
            } else {
                brls::Logger::debug("HomeDownloads::onDownloadProgress - skipping refresh for download {} (throttled)", id);
            }
        }
    });
}

void HomeDownloads::onDownloadComplete(const std::string& id, bool success) {
    // Usa sync per eseguire l'aggiornamento sul thread UI principale  
    brls::sync([this]() {
        if (shouldAutoRefresh.load()) {
            refresh();
        }
    });
}

void HomeDownloads::onDownloadItemSelected(const DownloadItem& item) {
    // Crea menu contestuale per il download
    brls::Dialog* dialog = new brls::Dialog(item.title);
    
    if (item.status == DownloadStatus::DOWNLOADING) {
        dialog->addButton("Pausa", [this, item]() {
            DownloadManager::instance().pauseDownload(item.id);
            this->refresh();
        });
    } else if (item.status == DownloadStatus::PAUSED) {
        dialog->addButton("Riprendi", [this, item]() {
            DownloadManager::instance().resumeDownload(item.id);
            this->refresh();
        });
    }
    
    if (item.status != DownloadStatus::COMPLETED) {
        dialog->addButton("Annulla", [this, item]() {
            DownloadManager::instance().cancelDownload(item.id);
            this->refresh();
        });
    }
    
    dialog->addButton("Elimina", [this, item]() {
        DownloadManager::instance().deleteDownload(item.id);
        this->refresh();
    });
    
    if (item.status == DownloadStatus::COMPLETED) {
        dialog->addButton("Riproduci", [this, item]() {
            // Verifica che il file esista usando std::ifstream invece di std::filesystem
            std::ifstream file(item.localPath);
            if (file.good()) {
                file.close();
                
                // Crea un canale fittizio per il file locale
                tsvitch::LiveM3u8 localChannel;
                localChannel.title = item.title;
                localChannel.url = "file://" + item.localPath;
                localChannel.logo = item.imageUrl; // Usa l'immagine del download
                
                std::vector<tsvitch::LiveM3u8> channelList = {localChannel};
                
                // Apri il player con il file locale
                Intent::openLive(channelList, 0, []() {
                    // Callback quando il player si chiude
                });
                
                brls::Logger::info("Playing local file: {}", item.localPath);
            } else {
                brls::Dialog* errorDialog = new brls::Dialog("Errore");
                errorDialog->addButton("OK", []() {});
                errorDialog->open();
                brls::Logger::error("File not found: {}", item.localPath);
            }
        });
    }
    
    dialog->addButton("Annulla", []() {});
    dialog->open();
}

brls::View* HomeDownloads::create() {
    return new HomeDownloads();
}
