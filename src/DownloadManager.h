#pragma once
#include <string>
#include <functional>
#include <atomic>
#include <cstdint>

class DownloadManager {
public:
    using ProgressFn = std::function<void(uint64_t got, uint64_t total, double bps)>;

    // Blocking download (call from worker thread)
    static bool Download(const std::string& url,
                         const std::string& outPath,
                         ProgressFn progress = nullptr,
                         std::atomic<bool>* cancel = nullptr);

    static bool DownloadToString(const std::string& url, std::string& out);

    static bool     EnsureDir(const std::string& path);
    static bool     FileExists(const std::string& path);
    static uint64_t FileSize(const std::string& path);
    static std::string Sha1OfFile(const std::string& path);
    static std::string Sha1OfString(const std::string& s);
};
