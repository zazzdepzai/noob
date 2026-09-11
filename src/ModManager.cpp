#include "ModManager.h"
#include "DownloadManager.h"
#include "Settings.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

void ModManager::loadFromConfig(const std::string& mcDir) {
    m_mods.clear();
    // Built-in default mod list
    m_mods.push_back({
        "ExampleMod", "examplemod-1.0.jar", "1.0", "GitHub Release",
        "https://raw.githubusercontent.com/zazzdepzai/RavenLauncherXD/main/ravenXD-v2.jar",
        true, false
    });

    // Optional: load extra mods from %APPDATA%\RavenXD\mods.json
    std::string modsJson = (fs::path(Settings::I().appDataDir()) / "mods.json").string();
    std::ifstream f(modsJson);
    if (f) {
        try {
            json j; f >> j;
            for (auto& e : j) {
                ModEntry m;
                m.name     = e.value("name","");
                m.fileName = e.value("fileName","");
                m.version  = e.value("version","1.0");
                m.source   = e.value("source","GitHub Release");
                m.url      = e.value("url","");
                m.enabled  = e.value("enabled", true);
                if (!m.name.empty()) m_mods.push_back(m);
            }
        } catch (...) {}
    }

    // check installed
    std::string modsDir = (fs::path(mcDir) / "mods").string();
    for (auto& m : m_mods) {
        m.installed = DownloadManager::FileExists((fs::path(modsDir) / m.fileName).string());
    }
}

bool ModManager::downloadMissing(const std::string& mcDir,
                                 std::function<void(const std::string&, float)> status)
{
    std::string modsDir = (fs::path(mcDir) / "mods").string();
    DownloadManager::EnsureDir(modsDir);
    int total = (int)m_mods.size(), done = 0;
    for (auto& m : m_mods) {
        std::string dst = (fs::path(modsDir) / m.fileName).string();
        if (!DownloadManager::FileExists(dst) && !m.url.empty()) {
            if (status) status("Downloading " + m.name + "...", (float)done / std::max(1,total));
            DownloadManager::Download(m.url, dst);
            m.installed = DownloadManager::FileExists(dst);
        }
        done++;
    }
    if (status) status("Mods ready", 1.f);
    return true;
}

bool ModManager::updateAll(const std::string& mcDir,
                           std::function<void(const std::string&, float)> status)
{
    // Simplest strategy: re-download all mods
    std::string modsDir = (fs::path(mcDir) / "mods").string();
    int total = (int)m_mods.size(), done = 0;
    for (auto& m : m_mods) {
        std::string dst = (fs::path(modsDir) / m.fileName).string();
        std::error_code ec; fs::remove(dst, ec);
        if (!m.url.empty()) {
            if (status) status("Updating " + m.name + "...", (float)done / std::max(1,total));
            DownloadManager::Download(m.url, dst);
        }
        m.installed = DownloadManager::FileExists(dst);
        done++;
    }
    return true;
}

void ModManager::setEnabled(size_t idx, bool en, const std::string& mcDir) {
    if (idx >= m_mods.size()) return;
    m_mods[idx].enabled = en;
    std::string modsDir = (fs::path(mcDir) / "mods").string();
    std::string on  = (fs::path(modsDir) / m_mods[idx].fileName).string();
    std::string off = on + ".disabled";
    std::error_code ec;
    if (en) { if (fs::exists(off)) fs::rename(off, on, ec); }
    else    { if (fs::exists(on))  fs::rename(on, off, ec); }
}

void ModManager::remove(size_t idx, const std::string& mcDir) {
    if (idx >= m_mods.size()) return;
    std::string modsDir = (fs::path(mcDir) / "mods").string();
    std::error_code ec;
    fs::remove((fs::path(modsDir) / m_mods[idx].fileName).string(), ec);
    fs::remove((fs::path(modsDir) / (m_mods[idx].fileName + ".disabled")).string(), ec);
    m_mods[idx].installed = false;
}
