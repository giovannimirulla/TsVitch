#pragma once

#include <fstream>
#include <string>

#include <borealis/core/application.hpp>
#include <borealis/core/logger.hpp>
#include <fmt/format.h>

#include "api/tsvitch/result/home_live_result.h"
#include "core/DownloadManager.hpp"
#include "core/DownloadProgressManager.hpp"
#include "utils/stream_helper.hpp"

namespace tsvitch {

inline void startChannelDownloadWithUi(const LiveM3u8& channel, const std::string& source) {
#ifdef __SWITCH__
    (void)channel;
    brls::Logger::info("{}: Switch ZR no-op download handler entered", source);
    brls::Logger::info("{}: Switch ZR no-op download handler returning", source);
    return;
#endif

    if (isXtreamSeriesPlaceholder(channel.url)) {
        brls::Logger::warning("{}: Cannot download Xtream series placeholder directly", source);
        showSeriesDownloadHint();
        return;
    }

    if (isLiveStream(channel.url, channel.title)) {
        brls::Logger::warning("{}: Cannot download live streams", source);
        showLiveStreamDownloadError();
        return;
    }

    std::string downloadId = DownloadManager::instance().startDownload(
        channel.title,
        channel.url,
        channel.logo,
#ifdef __SWITCH__
        nullptr,
        nullptr,
        nullptr);
#else
        [](const std::string& id, float progress, size_t downloaded, size_t total) {
            std::string progressText = fmt::format("{:.1f}%", progress);
            std::string statusText   = fmt::format("{} / {} bytes", downloaded, total);

            brls::sync([id, progress, progressText, statusText]() {
                DownloadProgressManager::getInstance()->updateProgress(id, progress, statusText, progressText);
            });
        },
        [](const std::string& id, const std::string& filePath) {
            brls::Logger::info("Download {} completed: {}", id, filePath);

            brls::sync([id, filePath]() {
                DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                brls::Application::notify(filePath != "Already completed" ? "Download completato!" : "File già scaricato!");
            });
        },
        [](const std::string& id, const std::string& error) {
            brls::Logger::error("Download {} failed: {}", id, error);
            brls::sync([id, error]() {
                DownloadProgressManager::getInstance()->hideDownloadProgress(id);
                brls::Application::notify("Errore download: " + error);
            });
        });
#endif

    if (downloadId.empty()) {
#ifndef __SWITCH__
        brls::Application::notify("Errore nell'avvio del download");
#endif
        brls::Logger::error("{}: Failed to start download for {}", source, channel.title);
        return;
    }

#ifndef __SWITCH__
    auto downloadItem = DownloadManager::instance().getDownload(downloadId);
    if (downloadItem.status == DownloadStatus::COMPLETED) {
        brls::Logger::info("{}: Skipped showing overlay for already completed download {} ({})", source, downloadId, channel.title);
        return;
    }

    DownloadProgressManager::getInstance()->showDownloadProgress(downloadId, channel.title, channel.url);
#endif
#ifndef __SWITCH__
    brls::Application::notify("Download avviato: " + channel.title);
#endif
    brls::Logger::info("{}: Started download {} for {}", source, downloadId, channel.title);
}

} // namespace tsvitch
