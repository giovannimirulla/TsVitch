#include "core/DownloadProgressManager.hpp"
#include "core/DownloadManager.hpp"
#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <borealis/core/thread.hpp>
#include <fmt/format.h>

namespace tsvitch {

// Static instance
DownloadProgressManager* DownloadProgressManager::instance = nullptr;

// Helper function to format bytes into human-readable format
std::string formatBytes(size_t bytes) {
    const double KB = 1024.0;
    const double MB = KB * 1024.0;
    const double GB = MB * 1024.0;
    
    if (bytes >= GB) {
        return fmt::format("{:.2f} GB", bytes / GB);
    } else if (bytes >= MB) {
        return fmt::format("{:.1f} MB", bytes / MB);
    } else if (bytes >= KB) {
        return fmt::format("{:.1f} KB", bytes / KB);
    } else {
        return fmt::format("{} B", bytes);
    }
}

// DownloadProgressOverlay implementation
DownloadProgressOverlay::DownloadProgressOverlay() {
    // Set up overlay styling
    this->setBackground(brls::ViewBackground::BACKDROP);
    this->setAlpha(0.95f);
    
    // Dimensions and layout
    this->setDimensions(400, 120);
    this->setJustifyContent(brls::JustifyContent::CENTER);
    this->setAlignItems(brls::AlignItems::CENTER);
    this->setAxis(brls::Axis::COLUMN);
    this->setPadding(20);
    this->setCornerRadius(10);
    
    // Position overlay at top-right corner (20px from edges)
    this->detach();
    // Use absolute positioning for detached view
    this->setPositionType(brls::PositionType::ABSOLUTE);
    int x = brls::Application::windowWidth - 400 - 20;
    int y = 20;
    this->setDetachedPosition((float)x, (float)y);

    // Download title
    downloadTitleLabel = new brls::Label();
    downloadTitleLabel->setFontSize(18);
    downloadTitleLabel->setTextColor(nvgRGB(255, 255, 255));
    downloadTitleLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    downloadTitleLabel->setText("Download in corso...");
    this->addView(downloadTitleLabel);
    
    // Status label
    downloadStatusLabel = new brls::Label();
    downloadStatusLabel->setFontSize(14);
    downloadStatusLabel->setTextColor(nvgRGB(200, 200, 200));
    downloadStatusLabel->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    downloadStatusLabel->setText("Inizializzazione...");
    downloadStatusLabel->setMarginTop(10);
    this->addView(downloadStatusLabel);
    
    // Progress bar
    downloadProgressBar = new brls::Slider();
    downloadProgressBar->setWidth(360);
    downloadProgressBar->setHeight(6);
    downloadProgressBar->setPointerSize(0.0f); // Hide cursor
    downloadProgressBar->setProgress(0.0f);
    downloadProgressBar->setMarginTop(15);
    this->addView(downloadProgressBar);
    
    // Progress text
    downloadProgressText = new brls::Label();
    downloadProgressText->setFontSize(12);
    downloadProgressText->setTextColor(nvgRGB(180, 180, 180));
    downloadProgressText->setHorizontalAlign(brls::HorizontalAlign::CENTER);
    downloadProgressText->setText("0%");
    downloadProgressText->setMarginTop(8);
    this->addView(downloadProgressText);
    
    // Initially hidden
    this->setVisibility(brls::Visibility::GONE);
    isVisible = false;
}

DownloadProgressOverlay::~DownloadProgressOverlay() {
    brls::Logger::debug("DownloadProgressOverlay destroyed");
}

void DownloadProgressOverlay::updateProgress(float progress, const std::string& status, const std::string& progressText) {
    if (downloadProgressBar) {
        downloadProgressBar->setProgress(progress / 100.0f);
    }
    
    // Aggiorna il titolo con la percentuale
    if (downloadTitleLabel) {
        std::string title = fmt::format("Download in corso... {}", progressText);
        downloadTitleLabel->setText(title);
    }
    
    if (downloadStatusLabel) {
        downloadStatusLabel->setText(status);
    }
    
    if (downloadProgressText) {
        downloadProgressText->setText(progressText);
    }
}

void DownloadProgressOverlay::show() {
    if (!isVisible) {
        this->setVisibility(brls::Visibility::VISIBLE);
        isVisible = true;
        brls::Logger::debug("DownloadProgressOverlay shown");
    }
}

void DownloadProgressOverlay::hide() {
    if (isVisible) {
        this->setVisibility(brls::Visibility::GONE);
        isVisible = false;
        brls::Logger::debug("DownloadProgressOverlay hidden");
    }
}

void DownloadProgressOverlay::setDownloadInfo(const std::string& title, const std::string& url) {
    if (downloadTitleLabel) {
        // Truncate title if too long
        std::string displayTitle = title;
        if (displayTitle.length() > 40) {
            displayTitle = displayTitle.substr(0, 37) + "...";
        }
        downloadTitleLabel->setText("Download: " + displayTitle);
    }
}

// DownloadProgressManager implementation
DownloadProgressManager::DownloadProgressManager() {
    // Set up dimensions similar to NotificationManager but full screen
    this->setWidth(brls::Application::windowWidth);
    this->setHeight(brls::Application::windowHeight);
    this->setDetachedPosition(0, 0);
    this->setVisibility(brls::Visibility::GONE);
    this->setBackground(brls::ViewBackground::NONE);
    
    brls::Logger::debug("DownloadProgressManager created");
}

DownloadProgressManager::~DownloadProgressManager() {
    if (currentOverlay) {
        this->removeView(currentOverlay.get());
        currentOverlay.reset();
    }
    brls::Logger::debug("DownloadProgressManager destroyed");
}

DownloadProgressManager* DownloadProgressManager::getInstance() {
    if (!instance) {
        instance = new DownloadProgressManager();
    }
    return instance;
}

void DownloadProgressManager::initialize() {
    brls::Logger::debug("DownloadProgressManager: initializing");
    
    // Set up fallback global callback in caso HomeDownloads non sia ancora inizializzato
    auto& manager = DownloadManager::instance();
    
    // Solo se non c'è già un callback impostato (HomeDownloads ha priorità)
    if (!manager.hasGlobalProgressCallback()) {
        brls::Logger::debug("DownloadProgressManager: setting fallback global progress callback");
        manager.setGlobalProgressCallback([this](const std::string& id, float progress, size_t downloaded, size_t total) {
            // Formatta le dimensioni in MB/GB usando la funzione helper
            std::string downloadedStr = formatBytes(downloaded);
            std::string totalStr = formatBytes(total);
            std::string progressText = fmt::format("{:.1f}%", progress);
            std::string statusText = fmt::format("{} / {}", downloadedStr, totalStr);
            
            // Se il download è completato, nascondi il banner
            if (progress >= 100.0f) {
                brls::Logger::info("DownloadProgressManager: Download {} completed, hiding progress banner", id);
                this->hideDownloadProgress(id);
            } else {
                this->updateProgress(id, progress, statusText, progressText);
            }
        });
    }
    
    isInitialized = true;
}

void DownloadProgressManager::cleanup() {
    brls::Logger::debug("DownloadProgressManager: cleaning up");
    
    // Hide any active downloads
    if (currentOverlay && !currentDownloadId.empty()) {
        hideDownloadProgress(currentDownloadId);
    }
    
    isInitialized = false;
    brls::Logger::debug("DownloadProgressManager: cleanup completed");
}

void DownloadProgressManager::showDownloadProgress(const std::string& downloadId, const std::string& title, const std::string& url) {
    brls::Logger::debug("DownloadProgressManager: showing progress for download {}", downloadId);
    
    if (!isInitialized) {
        brls::Logger::error("DownloadProgressManager: not initialized, cannot show download progress");
        return;
    }
    
    // Use brls::sync to ensure UI operations happen on the main thread
    brls::sync([this, downloadId, title, url]() {
        // If there's already an active download, hide it first
        if (currentOverlay && !currentDownloadId.empty()) {
            brls::Logger::warning("DownloadProgressManager: replacing existing download {} with {}", currentDownloadId, downloadId);
            hideDownloadProgress(currentDownloadId);
        }
        
        // Create new overlay
        currentOverlay = std::make_unique<DownloadProgressOverlay>();
        currentOverlay->setDownloadInfo(title, url);
        currentDownloadId = downloadId;
        
        // Add overlay to root view for absolute positioning
        auto activities = brls::Application::getActivitiesStack();
        if (!activities.empty()) {
            auto* activity = activities.back();
            auto* rootView = activity->getContentView();
            auto* rootBox = dynamic_cast<brls::Box*>(rootView);
            if (rootBox) {
                rootBox->addView(currentOverlay.get());
                currentOverlay->show();
                brls::Logger::debug("DownloadProgressManager: overlay added to root view");
            } else {
                brls::Logger::error("DownloadProgressManager: root view is not a Box");
            }
        } else {
            brls::Logger::error("DownloadProgressManager: no active activity");
        }
    });
}

void DownloadProgressManager::updateProgress(const std::string& downloadId, float progress, const std::string& status, const std::string& progressText) {
    if (currentOverlay && currentDownloadId == downloadId) {
        currentOverlay->updateProgress(progress, status, progressText);
    } else {
        brls::Logger::warning("DownloadProgressManager: trying to update progress for unknown download {}", downloadId);
    }
}

void DownloadProgressManager::hideDownloadProgress(const std::string& downloadId) {
    if (currentOverlay && currentDownloadId == downloadId) {
        brls::Logger::debug("DownloadProgressManager: hiding progress for download {}", downloadId);
        
        // Use brls::sync to ensure UI operations happen on the main thread
        brls::sync([this]() {
            currentOverlay->hide();
            
            // Remove overlay from its superview without freeing (unique_ptr owns it)
            currentOverlay->removeFromSuperView(false);
            brls::Logger::debug("DownloadProgressManager: overlay removed from superview");

            currentOverlay.reset();
            currentDownloadId.clear();
        });
    } else {
        // Only warn if we actually have an active download but it's a different one
        if (!currentDownloadId.empty() && currentDownloadId != downloadId) {
            brls::Logger::warning("DownloadProgressManager: trying to hide download {} but active download is {}", downloadId, currentDownloadId);
        } else {
            brls::Logger::debug("DownloadProgressManager: no active download to hide for {}", downloadId);
        }
    }
}

bool DownloadProgressManager::hasActiveDownloads() const {
    return isInitialized && currentOverlay && !currentDownloadId.empty();
}

} // namespace tsvitch

#if 0
// Duplicate implementations disabled
// ...excluded duplicate DownloadProgressOverlay and DownloadProgressManager implementations...
#endif
