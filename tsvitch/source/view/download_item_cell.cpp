#include "view/download_item_cell.hpp"
#include "utils/image_helper.hpp"
#include <borealis/core/i18n.hpp>
#include <fmt/format.h>
#include <algorithm>

using namespace brls::literals;

DownloadItemCell::DownloadItemCell() {
    brls::Logger::debug("DownloadItemCell constructor");
    this->inflateFromXMLRes("xml/views/download_item_cell.xml");
    this->reuseIdentifier = "DownloadItemCell";
    
    // Verifica che i componenti critici siano stati trovati e logga i loro stati
    brls::Logger::debug("DownloadItemCell: Component check - titleLabel: {}, statusLabel: {}, progressLabel: {}", 
        (titleLabel ? "OK" : "NULL"), (statusLabel ? "OK" : "NULL"), (progressLabel ? "OK" : "NULL"));
    brls::Logger::debug("DownloadItemCell: Component check - progressBar: {}, spinner: {}, imageView: {}, progressContainer: {}", 
        (progressBar ? "OK" : "NULL"), (spinner ? "OK" : "NULL"), (imageView ? "OK" : "NULL"), (progressContainer ? "OK" : "NULL"));
    
    if (!titleLabel || !statusLabel || !progressLabel || !progressBar || !spinner || !imageView || !progressContainer) {
        brls::Logger::error("DownloadItemCell: One or more components are null after inflation!");
    }
    
    // Configura lo slider senza cursore
    if (progressBar) {
        progressBar->setProgress(0.0f);
        progressBar->setPointerSize(0.0f);
        brls::Logger::debug("DownloadItemCell: Progress bar configured with initial progress 0.0");
    } else {
        brls::Logger::error("DownloadItemCell: Cannot configure progress bar - it's null!");
    }
    
    // Imposta la visibilità iniziale
    this->setVisibility(brls::Visibility::VISIBLE);
    brls::Logger::debug("DownloadItemCell constructor completed");
}

DownloadItemCell::~DownloadItemCell() {
    brls::Logger::debug("DownloadItemCell: delete");
    if (imageView) {
        ImageHelper::clear(imageView);
    }
}

void DownloadItemCell::setDownloadItem(const DownloadItem& item) {
    brls::Logger::debug("DownloadItemCell::setDownloadItem called for '{}' with status {} progress {:.1f}%", 
        item.title, static_cast<int>(item.status), item.progress);
    
    // Aggiorna TUTTO il currentItem prima di fare qualsiasi altra operazione
    currentItem = item;
    
    // Verifica che i componenti siano validi
    if (!titleLabel || !statusLabel) {
        brls::Logger::error("DownloadItemCell::setDownloadItem - null components!");
        return;
    }
    
    // Imposta il titolo (troncato se troppo lungo)
    std::string safeTitle = item.title;
    if (safeTitle.empty()) {
        safeTitle = "Download";
    } else if (safeTitle.length() > 45) {
        safeTitle = safeTitle.substr(0, 42) + "...";
    }
    titleLabel->setText(safeTitle);
    
    // Imposta l'immagine se disponibile
    if (imageView) {
        if (!item.imageUrl.empty()) {
            brls::Logger::debug("DownloadItemCell: Setting image URL: {}", item.imageUrl);
            ImageHelper::with(imageView)->load(item.imageUrl);
            imageView->setVisibility(brls::Visibility::VISIBLE);
        } else {
            ImageHelper::clear(imageView);
            imageView->setVisibility(brls::Visibility::GONE);
        }
    }
    
    // Aggiorna status e progress - IMPORTANTE: chiamare updateStatus prima di updateProgress
    updateStatus(item.status);
    
    // Aggiorna il progresso DOPO aver impostato lo status (che controlla la visibilità)
    updateProgress(item.progress);
    
    this->setVisibility(brls::Visibility::VISIBLE);
    brls::Logger::debug("DownloadItemCell item set successfully - downloadedSize: {}, totalSize: {}", 
        currentItem.downloadedSize, currentItem.totalSize);
}

void DownloadItemCell::updateProgress(float progress) {
    currentItem.progress = progress;
    
    // Debug: logga tutti i valori per il debug
    brls::Logger::debug("DownloadItemCell::updateProgress called with progress={:.1f}%, downloadedSize={}, totalSize={}", 
        progress, currentItem.downloadedSize, currentItem.totalSize);
    
    // Aggiorna sempre la progress bar se esiste, indipendentemente dalla visibilità
    if (progressBar) {
        float normalizedProgress = std::max(0.0f, std::min(100.0f, progress)) / 100.0f;
        progressBar->setProgress(normalizedProgress);
        brls::Logger::debug("DownloadItemCell: Updated progress bar to {:.1f}% (normalized: {:.3f})", progress, normalizedProgress);
    } else {
        brls::Logger::warning("DownloadItemCell: progressBar is null, cannot update progress");
    }
    
    // Aggiorna il testo del progresso se esiste
    if (progressLabel) {
        std::string progressText;
        if (currentItem.totalSize > 0) {
            std::string downloaded = formatFileSize(currentItem.downloadedSize);
            std::string total = formatFileSize(currentItem.totalSize);
            progressText = fmt::format("{:.1f}% ({} / {})", progress, downloaded, total);
        } else {
            progressText = fmt::format("{:.1f}%", progress);
        }
        progressLabel->setText(progressText);
        brls::Logger::debug("DownloadItemCell: Updated progress text to: {}", progressText);
    } else {
        brls::Logger::warning("DownloadItemCell: progressLabel is null, cannot update progress text");
    }
}

void DownloadItemCell::updateStatus(DownloadStatus status) {
    currentItem.status = status;
    brls::Logger::info("DownloadItemCell::updateStatus called with status: {}", static_cast<int>(status));
    
    // Aggiorna il testo dello status
    if (statusLabel) {
        statusLabel->setText(getStatusText(status));
    }
    
    // Mostra/nascondi componenti in base allo status
    bool showProgress = (status == DownloadStatus::DOWNLOADING || status == DownloadStatus::PENDING || status == DownloadStatus::PAUSED);
    
    if (progressContainer) {
        if (showProgress) {
            progressContainer->setVisibility(brls::Visibility::VISIBLE);
            brls::Logger::debug("DownloadItemCell: Showing progress container for status {}", static_cast<int>(status));
        } else {
            progressContainer->setVisibility(brls::Visibility::GONE);
            brls::Logger::info("DownloadItemCell: Hiding progress container for completed/failed download");
        }
    }
    
    // Gestisci lo spinner
    if (spinner) {
        if (status == DownloadStatus::DOWNLOADING || status == DownloadStatus::PENDING) {
            spinner->setVisibility(brls::Visibility::VISIBLE);
        } else {
            spinner->setVisibility(brls::Visibility::GONE);
        }
    }
}

RecyclingGridItem* DownloadItemCell::create() {
    brls::Logger::debug("DownloadItemCell::create() called - creating new cell");
    return new DownloadItemCell();
}

void DownloadItemCell::prepareForReuse() {
    brls::Logger::debug("DownloadItemCell::prepareForReuse called");
    RecyclingGridItem::prepareForReuse();
    
    // Reset visual state
    if (titleLabel) titleLabel->setText("Loading...");
    if (statusLabel) statusLabel->setText("Ready");
    if (progressLabel) progressLabel->setText("0%");
    if (progressBar) progressBar->setProgress(0.0f);
    if (spinner) spinner->setVisibility(brls::Visibility::GONE);
    if (progressContainer) progressContainer->setVisibility(brls::Visibility::GONE);
    if (imageView) {
        ImageHelper::clear(imageView);
        imageView->setVisibility(brls::Visibility::GONE);
    }
    
    this->setVisibility(brls::Visibility::VISIBLE);
}

void DownloadItemCell::cacheForReuse() {
    RecyclingGridItem::cacheForReuse();
}

std::string DownloadItemCell::formatFileSize(size_t bytes) {
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

std::string DownloadItemCell::getStatusText(DownloadStatus status) {
    switch (status) {
        case DownloadStatus::PENDING:
            return "In coda...";
        case DownloadStatus::DOWNLOADING:
            return "Download in corso...";
        case DownloadStatus::PAUSED:
            return "In pausa";
        case DownloadStatus::COMPLETED:
            return "Completato";
        case DownloadStatus::FAILED:
            if (!currentItem.error.empty()) {
                return fmt::format("Errore: {}", currentItem.error);
            }
            return "Download fallito";
        case DownloadStatus::CANCELLED:
            return "Annullato";
        default:
            return "Sconosciuto";
    }
}
