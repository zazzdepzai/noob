#include <windows.h>
#include "Launcher.h"
#include "DownloadManager.h"
#include "Minecraft.h"
#include "DiscordRPC.h"
#include <filesystem>
#include <thread>
#include <chrono>

namespace fs = std::filesystem;

// ===================== CTOR / DTOR =====================
Launcher::Launcher() = default;

Launcher::~Launcher() {
    if (m_worker.joinable()) m_worker.join();
}

// ===================== INIT =====================
void Launcher::init() {
    Settings::I().load();
    AccountManager::I().load();
    m_mods.loadFromConfig(Settings::I().d().minecraftDir);

    m_s.javaOk = !Minecraft::FindJava().empty();
    if (!m_s.javaOk) m_s.showJavaPopup = true;
}

// ===================== STATUS =====================
void Launcher::setStatus(const std::string& s, float pct, float bps) {
    std::lock_guard<std::mutex> lk(m_mtx);
    m_s.statusText = s;
    m_s.progress   = pct;
    m_s.speedMBps  = bps;
}

// ===================== TICK =====================
void Launcher::tick(float dt) {
    // Loading screen timer
    if (m_s.showLoadingScreen) {
        m_s.loadingTimer += dt;
        m_s.loadingPhase  = m_s.loadingTimer / 2.0f;
        if (m_s.loadingTimer >= 2.0f)
            m_s.showLoadingScreen = false;
    }
}

// ===================== LAUNCH BUTTON =====================
void Launcher::onLaunchClicked() {
    if (m_busy.load()) return;
    if (m_worker.joinable()) m_worker.join();

    m_busy.store(true);
    m_s.taskState  = TaskState::Running;
    m_s.lastError.clear();
    m_s.progress   = 0.f;
    m_s.speedMBps  = 0.f;
    m_s.statusText = "Preparing...";

    m_worker = std::thread([this]{ runLaunchTask(); });
}

// ===================== LAUNCH TASK =====================
void Launcher::runLaunchTask() {
    auto& d = Settings::I().d();

    auto status = [this](const std::string& phase, float pct) {
        setStatus(phase, pct, 0.f);
    };

    // Discord: launching
    DiscordRPC::I().setLaunching();

    // ============ 1. Java check ============
    std::string java = Minecraft::FindJava();
    if (java.empty()) {
        setStatus("Java not found", 0.f);
        m_s.taskState    = TaskState::Failed;
        m_s.lastError    = "Java not found. Please install Java 8 or select javaw.exe manually.";
        m_s.showJavaPopup = true;
        m_busy.store(false);
        DiscordRPC::I().setError("Java not found");
        return;
    }
    d.javaPath = java;
    Settings::I().save();

    // ============ 2. Vanilla Minecraft ============
    setStatus("Checking Minecraft 1.8.9...", 0.02f);
    if (!Minecraft::EnsureVanilla(d.minecraftDir, status)) {
        m_s.taskState = TaskState::Failed;
        m_s.lastError = "Failed to install Minecraft 1.8.9";
        m_busy.store(false);
        DiscordRPC::I().setError("MC install failed");
        return;
    }

    std::string versionId = "1.8.9";

    // ============ 3. Forge ============
    if (m_s.selectedVersion >= 1) {
        setStatus("Checking Forge 1.8.9...", 0.50f);
        std::string forgeId;
        if (!Minecraft::EnsureForge(d.minecraftDir, java, status, forgeId)) {
            m_s.taskState = TaskState::Failed;
            m_s.lastError = "Failed to install Forge 1.8.9. Check internet connection.";
            m_busy.store(false);
            DiscordRPC::I().setError("Forge failed");
            return;
        }
        versionId = forgeId;
    }

    // ============ 4. OptiFine ============
    if (m_s.selectedVersion >= 2) {
        setStatus("Checking OptiFine...", 0.80f);
        if (!Minecraft::EnsureOptiFine(d.minecraftDir, status)) {
            m_s.taskState = TaskState::Failed;
            m_s.lastError = "Failed to install OptiFine";
            m_busy.store(false);
            DiscordRPC::I().setError("OptiFine failed");
            return;
        }
    }

    // ============ 5. Mods ============
    setStatus("Downloading mods...", 0.90f);
    m_mods.downloadMissing(d.minecraftDir, [this](const std::string& s, float p){
        setStatus(s, 0.90f + 0.05f * p);
    });

    // ============ 6. Launch ============
    setStatus("Launching Minecraft...", 0.98f);

    // Lấy username từ AccountManager (nguồn chính xác nhất)
    std::string username = AccountManager::I().activeName();
    if (username.empty()) username = d.username.empty() ? "Player" : d.username;

    auto res = Minecraft::Launch(d.minecraftDir, java, versionId,
                                 username, d.ramMB,
                                 d.windowWidth, d.windowHeight);

    if (!res.ok) {
        m_s.taskState = TaskState::Failed;
        m_s.lastError = res.error;
        m_busy.store(false);
        DiscordRPC::I().setError("Launch failed");
        return;
    }

    // ============ 7. Discord: playing ============
    std::string verLabel;
    switch (m_s.selectedVersion) {
        case 0:  verLabel = "Minecraft 1.8.9";         break;
        case 1:  verLabel = "Forge 1.8.9";             break;
        case 2:  verLabel = "Forge 1.8.9 + OptiFine";  break;
        default: verLabel = "Minecraft 1.8.9";         break;
    }
    DiscordRPC::I().setPlaying(username, verLabel);

    setStatus("Ready to launch", 1.f);
    m_s.taskState = TaskState::Done;
    m_busy.store(false);

    if (d.closeAfterLaunch) {
        PostQuitMessage(0);
    }
}
