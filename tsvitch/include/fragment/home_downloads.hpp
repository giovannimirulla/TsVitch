#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/core/box.hpp>
#include "view/recycling_grid.hpp"

#include "core/DownloadManager.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <mutex>
#include <condition_variable>

class DownloadDataSource;

class HomeDownloads : public brls::Box {
public:
    HomeDownloads();
    ~HomeDownloads() override;

    void draw(NVGcontext* vg, float x, float y, float width, float height, brls::Style style, brls::FrameContext* ctx) override;
    void onFocusGained() override;
    void onFocusLost() override;

    void refresh();
    void forceRefresh(); // Forza un refresh immediato
    void onDownloadItemSelected(const DownloadItem& item);

    static brls::View* create();

private:
    BRLS_BIND(RecyclingGrid, recyclingGrid, "home/downloads/recyclingGrid");

    DownloadDataSource* dataSource = nullptr;
    std::atomic<bool> shouldAutoRefresh{true};
    std::atomic<bool> isShuttingDown{false}; // Flag per questa istanza
    std::thread refreshThread;
    std::atomic<bool> refreshThreadRunning{false};
    std::mutex refreshMutex;
    std::condition_variable refreshCondition;
    
    void setupRecyclingGrid();
    void startAutoRefresh();
    void stopAutoRefresh();
    void refreshWorker();
    void onDownloadProgress(const std::string& id, float progress);
    void onDownloadComplete(const std::string& id, bool success);
};
