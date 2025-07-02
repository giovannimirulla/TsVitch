#pragma once

#include <borealis/core/bind.hpp>
#include <borealis/views/label.hpp>
#include <borealis/views/progress_spinner.hpp>
#include <borealis/views/slider.hpp>
#include <borealis/views/image.hpp>
#include "view/recycling_grid.hpp"
#include "core/DownloadManager.hpp"

class DownloadItemCell : public RecyclingGridItem {
public:
    DownloadItemCell();
    ~DownloadItemCell() override;

    void setDownloadItem(const DownloadItem& item);
    void updateProgress(float progress);
    void updateStatus(DownloadStatus status);

    static RecyclingGridItem* create();

    void prepareForReuse() override;
    void cacheForReuse() override;

private:
    BRLS_BIND(brls::Label, titleLabel, "download_item/title");
    BRLS_BIND(brls::Label, statusLabel, "download_item/status");
    BRLS_BIND(brls::Label, progressLabel, "download_item/progress");
    BRLS_BIND(brls::Box, progressContainer, "download_item/progress_container");
    BRLS_BIND(brls::Slider, progressBar, "download_item/progress_bar");
    BRLS_BIND(brls::ProgressSpinner, spinner, "download_item/spinner");
    BRLS_BIND(brls::Image, imageView, "download_item/image");

    DownloadItem currentItem;
    std::string formatFileSize(size_t bytes);
    std::string getStatusText(DownloadStatus status);
};
