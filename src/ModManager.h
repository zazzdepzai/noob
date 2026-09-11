#pragma once
#include <string>
#include <vector>
#include <functional>

struct ModEntry {
    std::string name;
    std::string fileName;
    std::string version;
    std::string source;
    std::string url;
    bool enabled = true;
    bool installed = false;
};

class ModManager {
public:
    void loadFromConfig(const std::string& mcDir);
    bool downloadMissing(const std::string& mcDir,
                         std::function<void(const std::string&, float)> status);
    bool updateAll(const std::string& mcDir,
                   std::function<void(const std::string&, float)> status);
    void setEnabled(size_t idx, bool en, const std::string& mcDir);
    void remove(size_t idx, const std::string& mcDir);

    std::vector<ModEntry>& mods() { return m_mods; }

private:
    std::vector<ModEntry> m_mods;
};
