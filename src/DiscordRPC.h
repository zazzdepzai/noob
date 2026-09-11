#pragma once
#include <string>
#include <mutex>

#define RAVENXD_DISCORD_APP_ID "1547451186913878046"

class DiscordRPC {
public:
    static DiscordRPC& I();

    void init();
    void shutdown();

    void setIdle();
    void setLaunching();
    void setPlaying(const std::string& username, const std::string& version);
    void setError(const std::string& msg);

    bool isReady() const { return m_ready; }

private:
    DiscordRPC() = default;
    void update(const std::string& state,
                const std::string& details,
                const std::string& largeImg,
                const std::string& largeTxt,
                const std::string& smallImg,
                const std::string& smallTxt);

    bool m_ready = false;
    bool m_initialized = false;
    int64_t m_startTime = 0;
    std::mutex m_mtx;
};
