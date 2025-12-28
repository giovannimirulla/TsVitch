#include "core/ChannelManager.hpp"
#include "utils/config_helper.hpp"
#include <fstream>
#include <filesystem>
#include <cstdio>
#include <chrono>
#include <cstring>

using json = nlohmann::json;

ChannelManager::ChannelManager(const std::filesystem::path& dataDir) : 
    file_{dataDir / "channels.json"},
    binaryFile_{dataDir / "channels.bin"},
    timestampFile_{dataDir / "channels_timestamp.txt"} {}

ChannelManager* ChannelManager::get() {
    const std::string path = ProgramConfig::instance().getConfigDir();
    static ChannelManager instance(path);
    return &instance;
}

void ChannelManager::save(const tsvitch::LiveM3u8ListResult& channels) const {
    json j = channels;
    std::ofstream(file_) << j.dump(2);
}

void ChannelManager::saveWithTimestamp(const tsvitch::LiveM3u8ListResult& channels) const {
    brls::Logger::info("ChannelManager: Starting saveWithTimestamp for {} channels", channels.size());
    brls::Logger::info("ChannelManager: Binary cache file path: {}", binaryFile_.string());
    brls::Logger::info("ChannelManager: Timestamp file path: {}", timestampFile_.string());
    
    // Crea le directory se non esistono
    std::error_code ec;
    std::filesystem::create_directories(binaryFile_.parent_path(), ec);
    if (ec) {
        brls::Logger::error("ChannelManager: Failed to create directory: {}", ec.message());
        return;
    }
    
    brls::Logger::info("ChannelManager: Directory created successfully, saving channels in binary format...");
    
    // Salva i canali in formato binario (molto più veloce del JSON)
    saveBinary(channels);
    
    brls::Logger::info("ChannelManager: Binary cache saved, saving timestamp...");
    
    // Salva il timestamp
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    brls::Logger::info("ChannelManager: Saving timestamp: {}", timestamp);
    
    std::ofstream timestampStream(timestampFile_);
    if (!timestampStream) {
        brls::Logger::error("ChannelManager: Failed to open timestamp file for writing");
        return;
    }
    
    timestampStream << timestamp;
    timestampStream.flush();
    timestampStream.close();
    
    brls::Logger::info("ChannelManager: Binary cache and timestamp saved successfully");
    brls::Logger::info("ChannelManager: Verifying files exist...");
    
    if (std::filesystem::exists(binaryFile_)) {
        brls::Logger::info("ChannelManager: Binary cache file exists: {}", binaryFile_.string());
    } else {
        brls::Logger::error("ChannelManager: Binary cache file does not exist after save!");
    }
    
    if (std::filesystem::exists(timestampFile_)) {
        brls::Logger::info("ChannelManager: Timestamp file exists: {}", timestampFile_.string());
    } else {
        brls::Logger::error("ChannelManager: Timestamp file does not exist after save!");
    }
}

bool ChannelManager::isCacheValid(int maxAgeMinutes) const {
    brls::Logger::debug("ChannelManager: Checking cache validity...");
    brls::Logger::debug("ChannelManager: Binary cache file path: {}", binaryFile_.string());
    brls::Logger::debug("ChannelManager: Timestamp file path: {}", timestampFile_.string());
    
    // Preferisce la cache binaria se esiste, altrimenti fallback a JSON
    bool hasBinaryCache = std::filesystem::exists(binaryFile_);
    bool hasJsonCache = std::filesystem::exists(file_);
    
    if (!hasBinaryCache && !hasJsonCache) {
        brls::Logger::debug("ChannelManager: No cache files exist");
        return false;
    }
    
    if (hasBinaryCache) {
        brls::Logger::debug("ChannelManager: Binary cache found");
    } else {
        brls::Logger::debug("ChannelManager: Only JSON cache found (will be converted to binary on next save)");
    }
    
    if (!std::filesystem::exists(timestampFile_)) {
        brls::Logger::debug("ChannelManager: Timestamp file does not exist");
        return false;
    }
    
    std::ifstream timestampStream(timestampFile_);
    if (!timestampStream) {
        brls::Logger::debug("ChannelManager: Failed to open timestamp file");
        return false;
    }
    
    long long savedTimestamp;
    timestampStream >> savedTimestamp;
    brls::Logger::debug("ChannelManager: Saved timestamp: {}", savedTimestamp);
    
    auto now = std::chrono::system_clock::now();
    auto currentTimestamp = std::chrono::duration_cast<std::chrono::seconds>(now.time_since_epoch()).count();
    brls::Logger::debug("ChannelManager: Current timestamp: {}", currentTimestamp);
    
    auto ageInSeconds = currentTimestamp - savedTimestamp;
    auto maxAgeInSeconds = maxAgeMinutes * 60;
    
    brls::Logger::debug("ChannelManager: Cache age: {} seconds (max allowed: {} seconds)", ageInSeconds, maxAgeInSeconds);
    
    bool isValid = ageInSeconds <= maxAgeInSeconds;
    brls::Logger::debug("ChannelManager: Cache is {}", isValid ? "VALID" : "INVALID");
    
    return isValid;
}

tsvitch::LiveM3u8ListResult ChannelManager::loadIfValid(int maxAgeMinutes) const {
    brls::Logger::debug("ChannelManager: Attempting to load valid cache (max age: {} minutes)", maxAgeMinutes);
    
    if (isCacheValid(maxAgeMinutes)) {
        brls::Logger::debug("ChannelManager: Cache is valid, loading channels");
        
        // Preferisce la cache binaria se esiste, altrimenti fallback a JSON
        tsvitch::LiveM3u8ListResult result;
        if (std::filesystem::exists(binaryFile_)) {
            brls::Logger::debug("ChannelManager: Loading from binary cache (fast)");
            result = loadBinary();
        } else {
            brls::Logger::debug("ChannelManager: Loading from JSON cache (slow, will convert to binary)");
            result = load();
            // Converti a formato binario per il prossimo caricamento
            if (!result.empty()) {
                brls::Logger::info("ChannelManager: Converting JSON cache to binary format for faster future loads");
                saveBinary(result);
            }
        }
        
        brls::Logger::debug("ChannelManager: Loaded {} channels from cache", result.size());
        return result;
    }
    
    brls::Logger::debug("ChannelManager: Cache is invalid or empty, returning empty result");
    return {};
}

void ChannelManager::remove() const { 
    std::remove(file_.string().c_str());
    std::remove(binaryFile_.string().c_str());
    std::remove(timestampFile_.string().c_str());
}

tsvitch::LiveM3u8ListResult ChannelManager::load() const {
    tsvitch::LiveM3u8ListResult channels;
    try {
        if (std::ifstream in{file_}; in) {
            brls::Logger::debug("ChannelManager: Reading JSON cache file");
            json j;
            in >> j;
            brls::Logger::debug("ChannelManager: JSON parsed successfully");
            channels = j.get<tsvitch::LiveM3u8ListResult>();
            brls::Logger::debug("ChannelManager: Channels deserialized successfully");
        } else {
            brls::Logger::debug("ChannelManager: Could not open JSON cache file");
        }
    } catch (const std::exception& e) {
        brls::Logger::error("ChannelManager: Error loading JSON cache: {}", e.what());
        // Remove corrupted cache file
        remove();
    }
    return channels;
}

// Helper per scrivere stringhe in formato binario
void ChannelManager::writeString(std::ofstream& out, const std::string& str) const {
    uint32_t length = static_cast<uint32_t>(str.size());
    out.write(reinterpret_cast<const char*>(&length), sizeof(length));
    if (length > 0) {
        out.write(str.data(), length);
    }
}

// Helper per leggere stringhe in formato binario
std::string ChannelManager::readString(std::ifstream& in) const {
    uint32_t length = 0;
    in.read(reinterpret_cast<char*>(&length), sizeof(length));
    if (length == 0) {
        return "";
    }
    std::string str(length, '\0');
    in.read(&str[0], length);
    return str;
}

// Serializzazione binaria veloce (10-20x più veloce del JSON)
void ChannelManager::saveBinary(const tsvitch::LiveM3u8ListResult& channels) const {
    try {
        std::ofstream out(binaryFile_, std::ios::binary);
        if (!out) {
            brls::Logger::error("ChannelManager: Failed to open binary cache file for writing");
            return;
        }
        
        auto write_start = std::chrono::high_resolution_clock::now();
        
        // Scrivi il numero di canali
        uint32_t count = static_cast<uint32_t>(channels.size());
        out.write(reinterpret_cast<const char*>(&count), sizeof(count));
        
        // Buffer per accumulate dati prima di scrivere (riduce syscall da 3.6M a ~60k)
        std::vector<char> buffer;
        buffer.reserve(300 * 1024 * 1024); // Pre-alloca 300MB per evitare reallocazioni
        
        // Scrivi ogni canale nel buffer
        for (size_t i = 0; i < channels.size(); ++i) {
            const auto& channel = channels[i];
            
            // Scrivi nel buffer anziché nel file
            auto appendString = [&buffer](const std::string& str) {
                uint32_t length = static_cast<uint32_t>(str.size());
                const char* lenPtr = reinterpret_cast<const char*>(&length);
                buffer.insert(buffer.end(), lenPtr, lenPtr + sizeof(length));
                if (length > 0) {
                    buffer.insert(buffer.end(), str.begin(), str.end());
                }
            };
            
            appendString(channel.id);
            appendString(channel.chno);
            appendString(channel.title);
            appendString(channel.logo);
            appendString(channel.groupTitle);
            appendString(channel.url);
            
            // Scrivi il buffer al file ogni 5000 canali per non usare troppa RAM
            if ((i + 1) % 5000 == 0) {
                out.write(buffer.data(), buffer.size());
                buffer.clear();
                out.flush();
                std::this_thread::yield();
                brls::Logger::debug("ChannelManager: Written {} channels so far...", i + 1);
            }
        }
        
        // Scrivi i dati rimasti nel buffer
        if (!buffer.empty()) {
            out.write(buffer.data(), buffer.size());
        }
        
        out.flush();
        out.close();
        
        auto write_end = std::chrono::high_resolution_clock::now();
        auto write_duration = std::chrono::duration_cast<std::chrono::milliseconds>(write_end - write_start);
        brls::Logger::info("ChannelManager: Binary cache saved successfully ({} channels in {}ms)", count, write_duration.count());
    } catch (const std::exception& e) {
        brls::Logger::error("ChannelManager: Error saving binary cache: {}", e.what());
    }
}

// Deserializzazione binaria veloce (10-20x più veloce del JSON)
tsvitch::LiveM3u8ListResult ChannelManager::loadBinary() const {
    tsvitch::LiveM3u8ListResult channels;
    try {
        std::ifstream in(binaryFile_, std::ios::binary);
        if (!in) {
            brls::Logger::debug("ChannelManager: Could not open binary cache file");
            return channels;
        }
        
        // Leggi il numero di canali
        uint32_t count = 0;
        in.read(reinterpret_cast<char*>(&count), sizeof(count));
        
        if (count == 0 || count > 10000000) { // Sanity check
            brls::Logger::error("ChannelManager: Invalid channel count in binary cache: {}", count);
            return channels;
        }
        
        channels.reserve(count);
        
        // Leggi ogni canale
        for (uint32_t i = 0; i < count; ++i) {
            tsvitch::LiveM3u8 channel;
            channel.id = readString(in);
            channel.chno = readString(in);
            channel.title = readString(in);
            channel.logo = readString(in);
            channel.groupTitle = readString(in);
            channel.url = readString(in);
            channels.push_back(std::move(channel));
        }
        
        brls::Logger::info("ChannelManager: Binary cache loaded successfully ({} channels)", channels.size());
    } catch (const std::exception& e) {
        brls::Logger::error("ChannelManager: Error loading binary cache: {}", e.what());
        channels.clear();
    }
    return channels;
}