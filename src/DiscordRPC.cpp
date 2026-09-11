#include "DiscordRPC.h"
#include <discord_rpc.h>
#include <chrono>
#include <cstring>

// ---- Callbacks (chạy trong thread riêng của Discord SDK) ----
static void OnReady(const DiscordUser* user) {
    // connected
}
static void OnDisconnected(int code, const char* msg) {
    DiscordRPC::I().shutdown();
}
static void OnErrored(int code, const char* msg) {
    // ignore
}

DiscordRPC& DiscordRPC::I() {
    static DiscordRPC inst;
    return inst;
}

void DiscordRPC::init() {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (m_initialized) return;
    m_initialized = true;

    DiscordEventHandlers handlers{};
    handlers.ready        = OnReady;
    handlers.disconnected = OnDisconnected;
    handlers.errored      = OnErrored;

    // autoRegister = 1 → tự đăng ký protocol discord-<appid>
    Discord_Initialize(RAVENXD_DISCORD_APP_ID, &handlers, 1, nullptr);

    m_startTime = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    m_ready = true;
}

void DiscordRPC::shutdown() {
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_initialized) return;
    Discord_ClearPresence();
    Discord_Shutdown();
    m_initialized = false;
    m_ready = false;
}

void DiscordRPC::update(const std::string& state,
                        const std::string& details,
                        const std::string& largeImg,
                        const std::string& largeTxt,
                        const std::string& smallImg,
                        const std::string& smallTxt)
{
    std::lock_guard<std::mutex> lk(m_mtx);
    if (!m_ready) return;

    DiscordRichPresence p{};
    p.state          = state.empty()    ? nullptr : state.c_str();
    p.details        = details.empty()  ? nullptr : details.c_str();
    p.startTimestamp = m_startTime;
    p.largeImageKey  = largeImg.empty() ? nullptr : largeImg.c_str();
    p.largeImageText = largeTxt.empty() ? nullptr : largeTxt.c_str();
    p.smallImageKey  = smallImg.empty() ? nullptr : smallImg.c_str();
    p.smallImageText = smallTxt.empty() ? nullptr : smallTxt.c_str();
    p.instance = 0;

    Discord_UpdatePresence(&p);
}

void DiscordRPC::setIdle() {
    update("In Launcher",
           "Browsing RavenXD",
           "ravenxd", "RavenXD Launcher",
           "idle",    "Idle");
}

void DiscordRPC::setLaunching() {
    update("Launching Minecraft...",
           "Preparing to play",
           "ravenxd", "RavenXD Launcher",
           "loading", "Loading");
}

void DiscordRPC::setPlaying(const std::string& username, const std::string& version) {
    update("Playing " + version,
           "User: " + username,
           "ravenxd", "RavenXD Launcher",
           "play",    "In Game");
}

void DiscordRPC::setError(const std::string& msg) {
    update("Error",
           msg,
           "ravenxd", "RavenXD Launcher",
           "error",   "Error");
}
