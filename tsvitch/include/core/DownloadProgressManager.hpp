#pragma once

#include <borealis/core/box.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/slider.hpp>
#include <borealis/core/application.hpp>
#include <borealis/core/event.hpp>
#include <string>
#include <memory>

namespace tsvitch {

class DownloadProgressOverlay : public brls::Box {
public:
    explicit DownloadProgressOverlay();
    ~DownloadProgressOverlay() override;

    void updateProgress(float progress, const std::string& status, const std::string& progressText);
    void show();
    void hide();
    void setDownloadInfo(const std::string& title, const std::string& url);

private:
    brls::Label* downloadStatusLabel = nullptr;
    brls::Label* downloadProgressText = nullptr;
    brls::Label* downloadTitleLabel = nullptr;
    brls::Slider* downloadProgressBar = nullptr;
    
    bool isVisible = false;
};

class DownloadProgressManager : public brls::Box {
public:
    DownloadProgressManager();
    ~DownloadProgressManager() override;

    // Singleton pattern
    static DownloadProgressManager* getInstance();
    
    // Initialize the manager (must be called after Application is ready)
    void initialize();
    
    // Cleanup
    void cleanup();
    
    // Show progress for a download
    void showDownloadProgress(const std::string& downloadId, const std::string& title, const std::string& url);
    
    // Update progress
    void updateProgress(const std::string& downloadId, float progress, const std::string& status, const std::string& progressText);
    
    // Hide progress
    void hideDownloadProgress(const std::string& downloadId);
    
    // Check if any download is currently visible
    bool hasActiveDownloads() const;

private:
    static DownloadProgressManager* instance;
    std::unique_ptr<DownloadProgressOverlay> currentOverlay;
    std::string currentDownloadId;
    bool isInitialized = false;
};

} // namespace tsvitch
