#include "Settings.h"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

Settings& Settings::I() { static Settings s; return s; }

std::string Settings::appDataDir() const {
    char buf[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_APPDATA, nullptr, 0, buf))) {
        fs::path p = fs::path(buf) / "RavenXD";
        std::error_code ec; fs::create_directories(p, ec);
        return p.string();
    }
    return ".";
}

std::string Settings::configPath() const {
    return (fs::path(appDataDir()) / "config.json").string();
}

void Settings::load() {
    m_d.minecraftDir = (fs::path(appDataDir()) / "minecraft").string();
    std::ifstream f(configPath());
    if (!f) return;
    try {
        json j; f >> j;
        if (j.contains("minecraftDir"))    m_d.minecraftDir    = j["minecraftDir"].get<std::string>();
        if (j.contains("javaPath"))        m_d.javaPath        = j["javaPath"].get<std::string>();
        if (j.contains("ramMB"))           m_d.ramMB           = j["ramMB"].get<int>();
        if (j.contains("windowWidth"))     m_d.windowWidth     = j["windowWidth"].get<int>();
        if (j.contains("windowHeight"))    m_d.windowHeight    = j["windowHeight"].get<int>();
        if (j.contains("closeAfterLaunch"))m_d.closeAfterLaunch= j["closeAfterLaunch"].get<bool>();
        if (j.contains("debugMode"))       m_d.debugMode       = j["debugMode"].get<bool>();
        if (j.contains("username"))        m_d.username        = j["username"].get<std::string>();
        if (j.contains("version"))         m_d.version         = j["version"].get<std::string>();
    } catch (...) {}
    if (m_d.minecraftDir.empty())
        m_d.minecraftDir = (fs::path(appDataDir()) / "minecraft").string();
}

void Settings::save() {
    json j = {
        {"minecraftDir",     m_d.minecraftDir},
        {"javaPath",         m_d.javaPath},
        {"ramMB",            m_d.ramMB},
        {"windowWidth",      m_d.windowWidth},
        {"windowHeight",     m_d.windowHeight},
        {"closeAfterLaunch", m_d.closeAfterLaunch},
        {"debugMode",        m_d.debugMode},
        {"username",         m_d.username},
        {"version",          m_d.version},
    };
    std::ofstream f(configPath());
    if (f) f << j.dump(2);
}
