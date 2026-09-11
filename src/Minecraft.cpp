#include "Minecraft.h"
#include "Settings.h"
#include <windows.h>
#include <winhttp.h>
#include <shlobj.h>
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <cstring>
#include <filesystem>

#pragma comment(lib, "winhttp.lib")

namespace fs = std::filesystem;
using nlohmann::json;

// ============================================================
//  HTTP  (WinHTTP)
// ============================================================
static bool HttpGet(const std::string& url, std::string& out)
{
    out.clear();
    if (url.rfind("https://", 0) != 0 && url.rfind("http://", 0) != 0)
        return false;

    bool https = (url.rfind("https://", 0) == 0);
    size_t schemeEnd = https ? 8 : 7;
    size_t hostEnd   = url.find('/', schemeEnd);

    std::string host = (hostEnd == std::string::npos)
        ? url.substr(schemeEnd)
        : url.substr(schemeEnd, hostEnd - schemeEnd);
    std::string path = (hostEnd == std::string::npos) ? "/" : url.substr(hostEnd);

    std::wstring wHost(host.begin(), host.end());
    std::wstring wPath(path.begin(), path.end());

    HINTERNET hS = WinHttpOpen(L"RavenXD/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hS) return false;

    HINTERNET hC = WinHttpConnect(hS, wHost.c_str(),
        https ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT, 0);
    if (!hC) { WinHttpCloseHandle(hS); return false; }

    HINTERNET hR = WinHttpOpenRequest(hC, L"GET", wPath.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        https ? WINHTTP_FLAG_SECURE : 0);
    if (!hR) { WinHttpCloseHandle(hC); WinHttpCloseHandle(hS); return false; }

    BOOL ok = WinHttpSendRequest(hR, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                                 WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hR, nullptr);

    if (ok) {
        DWORD avail = 0;
        do {
            avail = 0;
            if (!WinHttpQueryDataAvailable(hR, &avail)) break;
            if (avail == 0) break;
            std::vector<char> buf(avail);
            DWORD read = 0;
            if (!WinHttpReadData(hR, buf.data(), avail, &read)) break;
            out.append(buf.data(), read);
        } while (avail > 0);
    }

    WinHttpCloseHandle(hR);
    WinHttpCloseHandle(hC);
    WinHttpCloseHandle(hS);
    return !out.empty();
}

static bool HttpDownloadToFile(const std::string& url, const std::string& dest,
                               const Minecraft::StatusFn& status,
                               const std::string& phase,
                               float pctStart, float pctEnd)
{
    // Tải toàn bộ vào RAM rồi ghi ra (đơn giản, đủ dùng cho file < 100MB)
    std::string data;
    if (status) status(phase, pctStart);
    if (!HttpGet(url, data)) return false;

    fs::create_directories(fs::path(dest).parent_path());

    std::ofstream f(dest, std::ios::binary);
    if (!f) return false;
    f.write(data.data(), (std::streamsize)data.size());
    if (!f.good()) return false;

    if (status) status(phase, pctEnd);
    return true;
}

// ============================================================
//  Helpers
// ============================================================
static bool FileExists(const std::string& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}
static bool DirExists(const std::string& p) {
    std::error_code ec;
    return fs::exists(p, ec) && fs::is_directory(p, ec);
}
static void EnsureDir(const std::string& p) {
    std::error_code ec;
    fs::create_directories(p, ec);
}

// Path separator cho classpath (Windows dùng ';')
static const char kPathSep = ';';

// ============================================================
//  FindJava
// ============================================================
std::string Minecraft::FindJava()
{
    // 1. Settings.javaPath nếu có
    {
        auto& s = Settings::I().d().javaPath;
        if (!s.empty() && FileExists(s)) return s;
    }

    // 2. JAVA_HOME
    {
        char buf[MAX_PATH] = {};
        DWORD n = GetEnvironmentVariableA("JAVA_HOME", buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            std::string cand = std::string(buf) + "\\bin\\javaw.exe";
            if (FileExists(cand)) return cand;
            cand = std::string(buf) + "\\bin\\java.exe";
            if (FileExists(cand)) return cand;
        }
    }

    // 3. PATH
    {
        char buf[MAX_PATH] = {};
        if (SearchPathA(nullptr, "javaw.exe", nullptr, MAX_PATH, buf, nullptr))
            return buf;
        if (SearchPathA(nullptr, "java.exe", nullptr, MAX_PATH, buf, nullptr))
            return buf;
    }

    // 4. Quét các thư mục phổ biến
    const char* roots[] = {
        "C:\\Program Files\\Java",
        "C:\\Program Files (x86)\\Java",
        "C:\\Program Files\\Eclipse Adoptium",
        "C:\\Program Files\\Microsoft",
        "C:\\Program Files\\BellSoft",
        "C:\\Program Files\\Zulu",
    };
    for (auto* root : roots) {
        std::error_code ec;
        if (!fs::is_directory(root, ec)) continue;
        for (auto& e : fs::directory_iterator(root, ec)) {
            if (!e.is_directory()) continue;
            std::string jw = (e.path() / "bin" / "javaw.exe").string();
            std::string j  = (e.path() / "bin" / "java.exe").string();
            if (FileExists(jw)) return jw;
            if (FileExists(j))  return j;
        }
    }

    return "";
}

// ============================================================
//  EnsureVanilla — tải 1.8.9.json + 1.8.9.jar + libraries + assets
// ============================================================
bool Minecraft::EnsureVanilla(const std::string& mcDir, StatusFn status)
{
    const std::string version = "1.8.9";

    std::string vDir  = mcDir + "\\versions\\" + version;
    std::string vJson = vDir + "\\" + version + ".json";
    std::string vJar  = vDir + "\\" + version + ".jar";

    EnsureDir(vDir);

    // --- 1. version.json ---
    if (!FileExists(vJson)) {
        if (status) status("Fetching version manifest...", 0.02f);

        std::string manifest;
        if (!HttpGet("https://launchermeta.mojang.com/mc/game/version_manifest.json", manifest))
            return false;

        json jm;
        try { jm = json::parse(manifest); }
        catch (...) { return false; }

        std::string versionUrl;
        for (auto& v : jm["versions"]) {
            if (v.value("id", "") == version) {
                versionUrl = v.value("url", "");
                break;
            }
        }
        if (versionUrl.empty()) return false;

        if (status) status("Downloading version.json...", 0.05f);
        std::string vjContent;
        if (!HttpGet(versionUrl, vjContent)) return false;

        std::ofstream f(vJson, std::ios::binary);
        if (!f) return false;
        f.write(vjContent.data(), (std::streamsize)vjContent.size());
    }

    // --- 2. Parse version.json ---
    json vj;
    try {
        std::ifstream f(vJson);
        vj = json::parse(f);
    } catch (...) { return false; }

    // --- 3. client.jar ---
    if (!FileExists(vJar)) {
        std::string clientUrl = vj["downloads"]["client"]["url"].get<std::string>();
        if (status) status("Downloading client.jar...", 0.08f);
        if (!HttpDownloadToFile(clientUrl, vJar, status,
                                "Downloading client.jar...", 0.08f, 0.30f))
            return false;
    }

    // --- 4. libraries ---
    std::string libsRoot = mcDir + "\\libraries";
    EnsureDir(libsRoot);

    auto& libs = vj["libraries"];
    int total = (int)libs.size();
    int done  = 0;
    for (auto& lib : libs) {
        ++done;
        float pct = 0.30f + 0.60f * (float)done / (float)std::max(1, total);

        // Chỉ tải library có artifact (bỏ native classifiers trừ khi cần)
        if (!lib.contains("downloads")) continue;
        auto& dl = lib["downloads"];

        if (dl.contains("artifact") && !dl["artifact"].is_null()) {
            auto& art = dl["artifact"];
            std::string path = art.value("path", "");
            std::string url  = art.value("url", "");
            if (path.empty() || url.empty()) continue;

            std::string dest = libsRoot + "\\" + path;
            // Chuyển '/' thành '\\' cho Windows
            for (auto& c : dest) if (c == '/') c = '\\';

            if (FileExists(dest)) continue;
            if (status) status("Downloading libraries...", pct);
            HttpDownloadToFile(url, dest, nullptr, "", 0.f, 0.f);
        }
    }

    // --- 5. assets index + assets ---
    std::string assetIndexId = vj.value("assets", "1.8");
    std::string assetsRoot    = mcDir + "\\assets";
    std::string indexesDir    = assetsRoot + "\\indexes";
    std::string objectsDir    = assetsRoot + "\\objects";
    EnsureDir(indexesDir);
    EnsureDir(objectsDir);

    std::string idxFile = indexesDir + "\\" + assetIndexId + ".json";
    if (!FileExists(idxFile)) {
        std::string idxUrl = vj["assetIndex"]["url"].get<std::string>();
        if (status) status("Downloading asset index...", 0.90f);
        if (!HttpDownloadToFile(idxUrl, idxFile, nullptr, "", 0.f, 0.f))
            return false;
    }

    // Tải assets (có thể rất nhiều — giới hạn 0.90 → 0.98)
    json aj;
    try { std::ifstream f(idxFile); aj = json::parse(f); }
    catch (...) { return false; }

    auto& objects = aj["objects"];
    int aTotal = (int)objects.size();
    int aDone  = 0;
    int aSkip  = 0;
    for (auto it = objects.begin(); it != objects.end(); ++it) {
        ++aDone;
        std::string hash = it.value().value("hash", "");
        if (hash.size() < 2) continue;

        std::string sub = hash.substr(0, 2);
        std::string dest = objectsDir + "\\" + sub + "\\" + hash;
        if (FileExists(dest)) { ++aSkip; continue; }

        // Chỉ tải một phần để tránh quá lâu (assets 1.8.9 ~ 3000 file)
        // Trong launcher thực tế nên dùng thread pool. Ở đây tải tuần tự.
        std::string url = "https://resources.download.minecraft.net/"
                        + sub + "/" + hash;

        if (status && (aDone % 50 == 0))
            status("Downloading assets...",
                   0.90f + 0.08f * (float)aDone / (float)std::max(1, aTotal));

        HttpDownloadToFile(url, dest, nullptr, "", 0.f, 0.f);
    }

    if (status) status("Vanilla ready", 1.0f);
    return FileExists(vJson) && FileExists(vJar);
}

// ============================================================
//  EnsureForge — FIX LỖI "version.json missing in installer"
// ============================================================
bool Minecraft::EnsureForge(const std::string& mcDir,
                            const std::string& javaPath,
                            StatusFn status,
                            std::string& outVersionId)
{
    const std::string forgeVersion = "1.8.9-forge1.8.9-11.15.1.2318";
    const std::string forgeDir = mcDir + "\\versions\\" + forgeVersion;
    const std::string forgeJson = forgeDir + "\\" + forgeVersion + ".json";
    const std::string forgeJar  = forgeDir + "\\" + forgeVersion + ".jar";

    outVersionId = forgeVersion;

    // Đã cài rồi?
    if (FileExists(forgeJson) && FileExists(forgeJar)) {
        if (status) status("Forge already installed", 1.0f);
        return true;
    }

    if (javaPath.empty() || !FileExists(javaPath)) {
        if (status) status("Java not found", 0.f);
        return false;
    }

    // ===== BƯỚC QUAN TRỌNG NHẤT =====
    // Forge installer cần vanilla 1.8.9 ĐÃ CÓ SẴN trong .minecraft.
    // Nếu thiếu version.json của vanilla → installer báo lỗi
    // "version.json missing in installer".
    if (!EnsureVanilla(mcDir, status)) {
        if (status) status("Failed to prepare vanilla 1.8.9", 0.f);
        return false;
    }

    // Tải Forge installer jar về thư mục tạm
    std::string tmpDir = mcDir + "\\.ravenxd_tmp";
    EnsureDir(tmpDir);
    std::string installerJar = tmpDir + "\\forge-1.8.9-installer.jar";

    if (!FileExists(installerJar)) {
        if (status) status("Downloading Forge installer...", 0.1f);
        std::string url =
            "https://maven.minecraftforge.net/net/minecraftforge/forge/"
            "1.8.9-11.15.1.2318/forge-1.8.9-11.15.1.2318-installer.jar";
        if (!HttpDownloadToFile(url, installerJar, status,
                                "Downloading Forge installer...", 0.1f, 0.4f))
            return false;
    }

    // Chạy installer silently
    if (status) status("Running Forge installer...", 0.5f);

    // Đường dẫn tuyệt đối — tránh lỗi khi CWD khác
    char mcDirAbs[MAX_PATH] = {};
    GetFullPathNameA(mcDir.c_str(), MAX_PATH, mcDirAbs, nullptr);

    std::string cmd = "\"" + javaPath + "\"";
    cmd += " -jar \"" + installerJar + "\"";
    cmd += " --installClient";
    cmd += " \"" + std::string(mcDirAbs) + "\"";

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, nullptr,
        &si, &pi);

    if (!ok) {
        if (status) status("Failed to start Forge installer", 0.f);
        return false;
    }

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD exitCode = 0;
    GetExitCodeProcess(pi.hProcess, &exitCode);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    if (status) status("Verifying Forge install...", 0.9f);

    if (!FileExists(forgeJson)) {
        if (status) status("Forge installer failed (no version.json)", 0.f);
        return false;
    }

    if (status) status("Forge ready", 1.0f);
    return true;
}

// ============================================================
//  EnsureOptiFine
// ============================================================
bool Minecraft::EnsureOptiFine(const std::string& mcDir, StatusFn status)
{
    std::string modsDir = mcDir + "\\mods";
    EnsureDir(modsDir);

    std::string optiJar = modsDir + "\\OptiFine_1.8.9_HD_U_M5.jar";
    if (FileExists(optiJar)) {
        if (status) status("OptiFine already present", 1.0f);
        return true;
    }

    if (status) status("Downloading OptiFine...", 0.2f);

    // OptiFine 1.8.9 HD U M5 — direct link từ optifine.net
    std::string url = "https://optifine.net/downloadx?f=OptiFine_1.8.9_HD_U_M5.jar&x=adf8f4a3d1e4b8c5";
    if (!HttpDownloadToFile(url, optiJar, status,
                            "Downloading OptiFine...", 0.2f, 0.95f))
    {
        // Fallback: link mirror
        url = "https://optifine.net/adloadx?f=OptiFine_1.8.9_HD_U_M5.jar";
        if (!HttpDownloadToFile(url, optiJar, status,
                                "Downloading OptiFine (mirror)...", 0.2f, 0.95f))
            return false;
    }

    if (status) status("OptiFine ready", 1.0f);
    return FileExists(optiJar);
}

// ============================================================
//  Launch — build classpath + spawn java
// ============================================================
LaunchResult Minecraft::Launch(const std::string& mcDir,
                               const std::string& javaPath,
                               const std::string& versionId,
                               const std::string& username,
                               int ramMB,
                               int winW, int winH)
{
    LaunchResult res;

    if (!FileExists(javaPath)) {
        res.error = "Java not found: " + javaPath;
        return res;
    }

    std::string vJsonPath = mcDir + "\\versions\\" + versionId + "\\" + versionId + ".json";
    if (!FileExists(vJsonPath)) {
        res.error = "version.json not found: " + vJsonPath;
        return res;
    }

    // Parse version.json
    json vj;
    try {
        std::ifstream f(vJsonPath);
        vj = json::parse(f);
    } catch (const std::exception& e) {
        res.error = std::string("Failed to parse version.json: ") + e.what();
        return res;
    }

    // Build classpath: client.jar + forge jar + all libraries
    std::vector<std::string> cpItems;

    // Vanilla client jar
    {
        std::string vanillaJar = mcDir + "\\versions\\1.8.9\\1.8.9.jar";
        if (FileExists(vanillaJar)) cpItems.push_back(vanillaJar);
    }

    // Forge jar
    {
        std::string forgeJar = mcDir + "\\versions\\" + versionId + "\\" + versionId + ".jar";
        if (FileExists(forgeJar)) cpItems.push_back(forgeJar);
    }

    // Libraries
    std::string libsRoot = mcDir + "\\libraries";
    if (vj.contains("libraries")) {
        for (auto& lib : vj["libraries"]) {
            if (!lib.contains("downloads")) continue;
            auto& dl = lib["downloads"];
            if (!dl.contains("artifact") || dl["artifact"].is_null()) continue;
            auto& art = dl["artifact"];
            std::string path = art.value("path", "");
            if (path.empty()) continue;
            std::string full = libsRoot + "\\" + path;
            for (auto& c : full) if (c == '/') c = '\\';
            if (FileExists(full)) cpItems.push_back(full);
        }
    }

    std::string classpath;
    for (size_t i = 0; i < cpItems.size(); ++i) {
        if (i) classpath += kPathSep;
        classpath += cpItems[i];
    }

    // Main class (Forge 1.8.9 dùng launchwrapper)
    std::string mainClass = vj.value("mainClass",
        "net.minecraft.launchwrapper.Launch");

    // UUID offline (deterministic từ username)
    std::string uuid;
    {
        // Đơn giản: hash username thành UUID format
        unsigned int h1 = 0x811c9dc5;
        for (char c : username) { h1 ^= (unsigned char)c; h1 *= 0x01000193; }
        unsigned int h2 = h1;
        for (int i = 0; i < 4; ++i) { h2 ^= h1; h2 *= 0x01000193; }
        char buf[64];
        snprintf(buf, sizeof(buf),
            "%08x-%04x-%04x-%04x-%08x%04x",
            h1,
            (h2 >> 16) & 0xFFFF,
            0x4000 | ((h1 >> 8) & 0x0FFF),
            0x8000 | (h2 & 0x3FFF),
            h2, h1 & 0xFFFF);
        uuid = buf;
    }

    // Command
    std::string cmd = "\"" + javaPath + "\"";
    cmd += " -Xmx" + std::to_string(ramMB) + "M";
    cmd += " -Xms" + std::to_string(std::min(ramMB, 512)) + "M";
    cmd += " -Djava.library.path=\"" + mcDir + "\\versions\\1.8.9\\natives\"";
    cmd += " -Dfml.ignoreInvalidMinecraftCertificates=true";
    cmd += " -Dfml.ignorePatchDiscrepancies=true";
    cmd += " -cp \"" + classpath + "\"";
    cmd += " " + mainClass;
    cmd += " --username \"" + username + "\"";
    cmd += " --version \"" + versionId + "\"";
    cmd += " --gameDir \"" + mcDir + "\"";
    cmd += " --assetsDir \"" + mcDir + "\\assets\"";
    cmd += " --assetIndex 1.8";
    cmd += " --uuid \"" + uuid + "\"";
    cmd += " --accessToken 0";
    cmd += " --userType legacy";
    cmd += " --width " + std::to_string(winW);
    cmd += " --height " + std::to_string(winH);

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back('\0');

    BOOL ok = CreateProcessA(
        nullptr, cmdBuf.data(),
        nullptr, nullptr, FALSE,
        CREATE_NO_WINDOW, nullptr, mcDir.c_str(),
        &si, &pi);

    if (!ok) {
        res.error = "Failed to start Java process (error " +
                    std::to_string(GetLastError()) + ")";
        return res;
    }

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    res.ok = true;
    return res;
}
