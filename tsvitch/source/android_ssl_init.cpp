/**
 * android_ssl.cpp
 * 
 * Android SSL/TLS initialization for TsVitch.
 * Extracts the Mozilla CA bundle from libromfs to internal storage
 * and configures cpr/curl to use it for HTTPS requests.
 */

#ifdef __ANDROID__

#include <unistd.h>
#include <fstream>
#include <string>
#include <android/log.h>

#include <romfs/romfs.hpp>
#include <borealis/core/logger.hpp>

#include "api/tsvitch/util/http.hpp"

#define LOG_TAG "TsVitch-SSL"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

namespace tsvitch {

void HTTP::initAndroidSSL(const std::string& configDir) {
    std::string caPath = configDir + "/cacert.pem";

    // Extract cacert.pem from libromfs to internal storage if not already there
    if (access(caPath.c_str(), R_OK) != 0) {
        LOGI("Extracting CA bundle to %s", caPath.c_str());

        try {
            // Read from libromfs (bundled resources)
            const auto& resource = romfs::get("cacert.pem");
            if (resource.valid()) {
                // Write to filesystem
                std::ofstream dst(caPath, std::ios::binary);
                if (dst.is_open()) {
                    dst.write(reinterpret_cast<const char*>(resource.data()), resource.size());
                    dst.close();
                    LOGI("CA bundle extracted successfully (%zu bytes)", resource.size());
                } else {
                    LOGE("Failed to write CA bundle to %s", caPath.c_str());
                }
            } else {
                LOGE("CA bundle resource is invalid in libromfs");
            }
        } catch (const std::exception& e) {
            LOGE("Failed to read CA bundle from libromfs: %s", e.what());
        }
    } else {
        LOGI("CA bundle already exists at %s", caPath.c_str());
    }

    // Configure SSL options with the CA bundle
    if (access(caPath.c_str(), R_OK) == 0) {
        HTTP::SSL_OPTIONS = cpr::Ssl(cpr::ssl::CaInfo{caPath});
        LOGI("SSL configured with CA bundle: %s", caPath.c_str());
    } else {
        LOGE("CA bundle not available, HTTPS may fail");
        // Fallback: disable peer verification (not recommended for production)
        HTTP::SSL_OPTIONS = cpr::Ssl(cpr::ssl::VerifyPeer{false});
    }
}

} // namespace tsvitch

#endif // __ANDROID__
