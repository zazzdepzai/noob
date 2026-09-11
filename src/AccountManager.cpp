#include "AccountManager.h"
#include "Settings.h"
#include "DownloadManager.h"
#include <filesystem>
#include <fstream>
#include <random>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

AccountManager& AccountManager::I() {
    static AccountManager m;
    return m;
}

static std::string genUuid() {
    std::random_device rd;
    std::mt19937_64 gen(rd());
    std::uniform_int_distribution<int> d(0, 15);
    const char* hex = "0123456789abcdef";
    std::string u;
    for (int i = 0; i < 32; ++i) u += hex[d(gen)];
    return u;
}

std::string AccountManager::activeName() const {
    for (auto& a : m_list) if (a.active) return a.name;
    return "Player";
}

int AccountManager::activeIndex() const {
    for (size_t i = 0; i < m_list.size(); ++i)
        if (m_list[i].active) return (int)i;
    return -1;
}

void AccountManager::load() {
    m_list.clear();
    std::string p = (fs::path(Settings::I().appDataDir()) / "accounts.json").string();
    std::ifstream f(p);
    if (f) {
        try {
            json j; f >> j;
            for (auto& e : j) {
                Account a;
                a.name     = e.value("name","Player");
                a.uuid     = e.value("uuid", genUuid());
                a.active   = e.value("active", false);
                a.colorIdx = e.value("colorIdx", 0);
                m_list.push_back(a);
            }
        } catch (...) {}
    }

    // Default account nếu trống
    if (m_list.empty()) {
        Account a;
        a.name     = Settings::I().d().username.empty() ? "Player" : Settings::I().d().username;
        a.uuid     = genUuid();
        a.active   = true;
        a.colorIdx = 0;
        m_list.push_back(a);
        save();
    }

    // Đảm bảo có 1 account active
    bool has = false;
    for (auto& a : m_list) if (a.active) { has = true; break; }
    if (!has && !m_list.empty()) m_list[0].active = true;

    // Sync với Settings
    Settings::I().d().username = activeName();
}

void AccountManager::save() {
    std::string p = (fs::path(Settings::I().appDataDir()) / "accounts.json").string();
    json j = json::array();
    for (auto& a : m_list) {
        j.push_back({
            {"name",     a.name},
            {"uuid",     a.uuid},
            {"active",   a.active},
            {"colorIdx", a.colorIdx}
        });
    }
    std::ofstream f(p);
    if (f) f << j.dump(2);
}

void AccountManager::add(const std::string& name) {
    Account a;
    a.name     = name.empty() ? "Player" : name;
    a.uuid     = genUuid();
    a.active   = m_list.empty();  // account đầu tiên → active
    a.colorIdx = (int)m_list.size() % 8;
    m_list.push_back(a);
    save();
}

void AccountManager::remove(size_t idx) {
    if (idx >= m_list.size()) return;
    bool wasActive = m_list[idx].active;
    m_list.erase(m_list.begin() + idx);

    if (wasActive && !m_list.empty()) m_list[0].active = true;
    save();

    Settings::I().d().username = activeName();
    Settings::I().save();
}

void AccountManager::setActive(size_t idx) {
    if (idx >= m_list.size()) return;
    for (size_t i = 0; i < m_list.size(); ++i)
        m_list[i].active = (i == idx);
    save();
    Settings::I().d().username = activeName();
    Settings::I().save();
}

void AccountManager::rename(size_t idx, const std::string& newName) {
    if (idx >= m_list.size()) return;
    m_list[idx].name = newName.empty() ? "Player" : newName;
    save();
    if (m_list[idx].active) {
        Settings::I().d().username = newName;
        Settings::I().save();
    }
}

void AccountManager::avatarColor(int idx, int& r, int& g, int& b) {
    static const int colors[8][3] = {
        { 80, 140, 245}, {220,  95, 120}, { 80, 200, 150},
        {240, 170,  60}, {170, 110, 230}, { 90, 200, 220},
        {230, 110, 180}, {140, 200,  90}
    };
    int i = idx % 8;
    if (i < 0) i += 8;
    r = colors[i][0]; g = colors[i][1]; b = colors[i][2];
}
