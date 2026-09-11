#pragma once
#include <string>

struct SettingsData {
    std::string minecraftDir;
    std::string javaPath;
    int  ramMB         = 2048;
    int  windowWidth   = 854;
    int  windowHeight  = 480;
    bool closeAfterLaunch = false;
    bool debugMode     = false;
    std::string username = "Player";
    std::string version  = "Forge 1.8.9 + OptiFine";
};

class Settings {
public:
    static Settings& I();
    void load();
    void save();
    SettingsData& d() { return m_d; }
    std::string appDataDir() const;   // %APPDATA%\RavenXD
    std::string configPath() const;
private:
    SettingsData m_d;
};
