#pragma once
#include <string>
#include <vector>
#include <atomic>
#include <functional>
#include <nlohmann/json.hpp>

struct ProgressState {
    std::atomic<float> percent{0.f};
    std::atomic<float> speedMBps{0.f};
    std::string phase;              // guarded by mutex in Launcher
};

struct LaunchResult {
    bool ok = false;
    std::string error;
};

class Minecraft {
public:
    using StatusFn = std::function<void(const std::string& phase, float pct)>;

    // Ensure vanilla 1.8.9 exists (client.jar, libraries, assets)
    static bool EnsureVanilla(const std::string& mcDir, StatusFn status);

    // Ensure Forge 1.8.9 exists (runs installer silently)
    static bool EnsureForge(const std::string& mcDir, const std::string& javaPath,
                            StatusFn status, std::string& outVersionId);

    // Ensure OptiFine mod is present in mods/
    static bool EnsureOptiFine(const std::string& mcDir, StatusFn status);

    // Build classpath + launch. versionId: e.g. "1.8.9-forge1.8.9-11.15.1.2318-1.8.9"
    static LaunchResult Launch(const std::string& mcDir,
                               const std::string& javaPath,
                               const std::string& versionId,
                               const std::string& username,
                               int ramMB,
                               int winW, int winH);

    // Find Java: prefers Settings.javaPath, then JAVA_HOME, then PATH
    static std::string FindJava();
};