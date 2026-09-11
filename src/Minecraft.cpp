#define NOMINMAX
#include <windows.h>
#include "Minecraft.h"
#include "DownloadManager.h"
#include "Settings.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <thread>

namespace fs = std::filesystem;
using json = nlohmann::json;

static const char* MANIFEST_URL =
    "https://launchermeta.mojang.com/mc/game/version_manifest_v2.json";
static const char* RESOURCES_BASE =
    "https://resources.download.minecraft.net/";

static std::string joinPath(const std::string& a, const std::string& b) {
    return (fs::path(a) / b).string();
}

// ==================== HELPERS ====================
static int RunProcessCapture(const std::string& cmd,
                             const std::string& workDir,
                             const std::string& logFile)
{
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof(sa);
    sa.bInheritHandle = TRUE;

    HANDLE hOut = CreateFileA(logFile.c_str(), GENERIC_WRITE, FILE_SHARE_READ,
                              &sa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hOut == INVALID_HANDLE_VALUE) hOut = nullptr;

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = hOut ? hOut : GetStdHandle(STD_OUTPUT_HANDLE);
    si.hStdError  = hOut ? hOut : GetStdHandle(STD_ERROR_HANDLE);
    si.hStdInput  = GetStdHandle(STD_INPUT_HANDLE);

    PROCESS_INFORMATION pi{};
    std::vector<char> cmdBuf(cmd.begin(), cmd.end());
    cmdBuf.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdBuf.data(), nullptr, nullptr, TRUE,
                             CREATE_NO_WINDOW, nullptr,
                             workDir.empty() ? nullptr : workDir.c_str(),
                             &si, &pi);
    if (hOut) CloseHandle(hOut);
    if (!ok) return -1;

    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return (int)code;
}

static bool UnzipFile(const std::string& zipPath, const std::string& outDir) {
    DownloadManager::EnsureDir(outDir);
    std::string ps =
        "powershell -NoProfile -Command \""
        "Add-Type -A System.IO.Compression.FileSystem; "
        "if (Test-Path '" + outDir + "') { Remove-Item -Recurse -Force '" + outDir + "' }; "
        "New-Item -ItemType Directory -Force -Path '" + outDir + "' | Out-Null; "
        "[IO.Compression.ZipFile]::ExtractToDirectory('" + zipPath + "','" + outDir + "')\"";
    std::string log = joinPath(Settings::I().appDataDir(), "unzip.log");
    int code = RunProcessCapture(ps, "", log);
    return code == 0;
}

// ==================== JAVA FINDER ====================
std::string Minecraft::FindJava() {
    auto& d = Settings::I().d();
    if (!d.javaPath.empty() && DownloadManager::FileExists(d.javaPath)) return d.javaPath;

    char buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableA("JAVA_HOME", buf, MAX_PATH);
    if (n > 0) {
        std::string p = joinPath(buf, "bin\\javaw.exe");
        if (DownloadManager::FileExists(p)) return p;
        p = joinPath(buf, "bin\\java.exe");
        if (DownloadManager::FileExists(p)) return p;
    }
    const char* guesses[] = {
        "C:\\Program Files\\Java\\jre1.8.0_432\\bin\\javaw.exe",
        "C:\\Program Files\\Java\\jre1.8.0_431\\bin\\javaw.exe",
        "C:\\Program Files\\Java\\jre1.8.0_421\\bin\\javaw.exe",
        "C:\\Program Files\\Java\\jre1.8.0_411\\bin\\javaw.exe",
        "C:\\Program Files\\Java\\jre1.8.0_401\\bin\\javaw.exe",
        "C:\\Program Files\\Java\\jre1.8.0_391\\bin\\javaw.exe",
        "C:\\Program Files (x86)\\Java\\jre1.8.0_432\\bin\\javaw.exe",
        "C:\\Program Files (x86)\\Java\\jre1.8.0_401\\bin\\javaw.exe",
        "C:\\Program Files\\Eclipse Adoptium\\jdk-8.0.432.6-hotspot\\bin\\javaw.exe",
        "C:\\Program Files\\Eclipse Adoptium\\jre-8.0.432.6-hotspot\\bin\\javaw.exe",
        "C:\\Program Files\\Eclipse Adoptium\\jdk-8.0.402.6-hotspot\\bin\\javaw.exe",
        "C:\\Program Files\\Eclipse Adoptium\\jre-8.0.402.6-hotspot\\bin\\javaw.exe",
        "C:\\Program Files\\Microsoft\\jdk-8.0.402.6-hotspot\\bin\\javaw.exe",
    };
    for (auto g : guesses) if (DownloadManager::FileExists(g)) return g;

    char pathBuf[32768];
    if (GetEnvironmentVariableA("PATH", pathBuf, sizeof(pathBuf))) {
        std::stringstream ss(pathBuf);
        std::string seg;
        while (std::getline(ss, seg, ';')) {
            std::string p = joinPath(seg, "javaw.exe");
            if (DownloadManager::FileExists(p)) return p;
            p = joinPath(seg, "java.exe");
            if (DownloadManager::FileExists(p)) return p;
        }
    }
    return "";
}

// ==================== MANIFEST / VERSION JSON ====================
static bool fetchManifest(json& out) {
    std::string body;
    if (!DownloadManager::DownloadToString(MANIFEST_URL, body)) return false;
    try { out = json::parse(body); } catch (...) { return false; }
    return true;
}

static bool fetchVersionJson(const std::string& url, json& out) {
    std::string body;
    if (!DownloadManager::DownloadToString(url, body)) return false;
    try { out = json::parse(body); } catch (...) { return false; }
    return true;
}

// ==================== LIBRARIES ====================
static void downloadAllLibraries(const std::string& mcDir, const json& vjson,
                                 Minecraft::StatusFn st, float base, float span)
{
    if (!vjson.contains("libraries")) return;
    auto libs = vjson["libraries"];
    int total = (int)libs.size();
    int done = 0;
    for (auto& lib : libs) {
        if (lib.contains("rules")) {
            bool allow = false;
            for (auto& r : lib["rules"]) {
                bool osMatch = true;
                if (r.contains("os")) {
                    std::string osName = r["os"].value("name","");
                    if (!osName.empty() && osName != "windows") osMatch = false;
                }
                if (osMatch) allow = (r.value("action","allow") == "allow");
            }
            if (!allow) { done++; continue; }
        }
        if (lib.contains("natives")) {
            std::string cls = lib["natives"].value("windows","");
            if (!cls.empty() && lib.contains("downloads") && lib["downloads"].contains("classifiers") &&
                lib["downloads"]["classifiers"].contains(cls))
            {
                auto& nv = lib["downloads"]["classifiers"][cls];
                std::string rel = nv.value("path","");
                std::string url = nv.value("url","");
                std::string dst = joinPath(mcDir, "libraries/" + rel);
                if (!DownloadManager::FileExists(dst) && !url.empty())
                    DownloadManager::Download(url, dst);
            }
        }
        if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
            auto& art = lib["downloads"]["artifact"];
            std::string rel = art.value("path","");
            std::string url = art.value("url","");
            std::string dst = joinPath(mcDir, "libraries/" + rel);
            if (!DownloadManager::FileExists(dst) && !url.empty())
                DownloadManager::Download(url, dst);
        }
        done++;
        if (st) st("Downloading libraries...",
                   base + span * (float)done / (std::max)(1,total));
    }
}

// ==================== ASSETS ====================
static void downloadAssets(const std::string& mcDir, const json& vjson,
                           Minecraft::StatusFn st, float base, float span)
{
    if (!vjson.contains("assetIndex")) return;
    auto& ai = vjson["assetIndex"];
    std::string idxUrl  = ai.value("url","");
    std::string idxId   = ai.value("id","");
    std::string idxDst  = joinPath(mcDir, "assets/indexes/" + idxId + ".json");
    if (!DownloadManager::FileExists(idxDst)) {
        DownloadManager::EnsureDir(joinPath(mcDir, "assets/indexes"));
        if (!idxUrl.empty()) DownloadManager::Download(idxUrl, idxDst);
    }
    std::ifstream f(idxDst);
    if (!f) return;
    json idx; try { f >> idx; } catch (...) { return; }
    if (!idx.contains("objects")) return;
    auto& objs = idx["objects"];
    int total = (int)objs.size(), done = 0;
    for (auto it = objs.begin(); it != objs.end(); ++it) {
        std::string hash = it.value().value("hash","");
        if (hash.size() < 2) { done++; continue; }
        std::string sub = hash.substr(0, 2);
        std::string url = std::string(RESOURCES_BASE) + sub + "/" + hash;
        std::string dst = joinPath(mcDir, "assets/objects/" + sub + "/" + hash);
        if (!DownloadManager::FileExists(dst))
            DownloadManager::Download(url, dst);
        done++;
        if ((done % 50) == 0 && st)
            st("Downloading assets...",
               base + span * (float)done / (std::max)(1,total));
    }
}

// ==================== VANILLA ====================
bool Minecraft::EnsureVanilla(const std::string& mcDir, StatusFn st) {
    DownloadManager::EnsureDir(mcDir);
    DownloadManager::EnsureDir(joinPath(mcDir, "versions"));
    DownloadManager::EnsureDir(joinPath(mcDir, "libraries"));
    DownloadManager::EnsureDir(joinPath(mcDir, "assets"));
    DownloadManager::EnsureDir(joinPath(mcDir, "mods"));

    if (st) st("Fetching version manifest...", 0.02f);
    json manifest;
    if (!fetchManifest(manifest)) return false;

    std::string vjsonUrl;
    for (auto& v : manifest["versions"]) {
        if (v.value("id","") == "1.8.9") { vjsonUrl = v.value("url",""); break; }
    }
    if (vjsonUrl.empty()) return false;

    if (st) st("Fetching version metadata...", 0.05f);
    json vjson;
    if (!fetchVersionJson(vjsonUrl, vjson)) return false;

    std::string clientUrl = vjson["downloads"]["client"].value("url","");
    std::string verDir = joinPath(mcDir, "versions/1.8.9");
    DownloadManager::EnsureDir(verDir);
    std::string clientDst = joinPath(verDir, "1.8.9.jar");
    if (!DownloadManager::FileExists(clientDst) && !clientUrl.empty()) {
        if (st) st("Downloading Minecraft client...", 0.10f);
        DownloadManager::Download(clientUrl, clientDst);
    }
    std::string vjsonDst = joinPath(verDir, "1.8.9.json");
    { std::ofstream o(vjsonDst); o << vjson.dump(); }

    downloadAllLibraries(mcDir, vjson, st, 0.15f, 0.55f);
    downloadAssets(mcDir, vjson, st, 0.70f, 0.98f);
    if (st) st("Ready", 1.f);
    return true;
}

// ==================== FORGE (FIX: extract thủ công, không chạy Java installer) ====================
bool Minecraft::EnsureForge(const std::string& mcDir, const std::string& javaPath,
                            StatusFn st, std::string& outVersionId)
{
    (void)javaPath;  // không cần Java để extract ZIP
    const std::string fv = "1.8.9-11.15.1.2318-1.8.9";
    outVersionId = "1.8.9-forge1.8.9-11.15.1.2318-1.8.9";

    std::string verDir = joinPath(mcDir, "versions/" + outVersionId);
    std::string marker = joinPath(verDir, outVersionId + ".json");

    if (DownloadManager::FileExists(marker)) return true;

    DownloadManager::EnsureDir(verDir);

    // ---- Bước 1: Download installer ----
    std::string instUrl =
        "https://maven.minecraftforge.net/net/minecraftforge/forge/"
        + fv + "/forge-" + fv + "-installer.jar";
    std::string instPath = joinPath(Settings::I().appDataDir(), "forge-installer.jar");

    if (st) st("Downloading Forge installer...", 0.20f);
    if (!DownloadManager::Download(instUrl, instPath)) {
        if (st) st("ERROR: Cannot download Forge installer", 0.25f);
        return false;
    }
    if (!DownloadManager::FileExists(instPath)) {
        if (st) st("ERROR: Forge installer not found", 0.25f);
        return false;
    }

    // ---- Bước 2: Extract installer như ZIP (installer.jar là file ZIP) ----
    std::string extractDir = joinPath(Settings::I().appDataDir(), "forge-extract");
    if (st) st("Extracting Forge installer...", 0.40f);
    if (!UnzipFile(instPath, extractDir)) {
        if (st) st("ERROR: Cannot extract installer", 0.45f);
        return false;
    }

    // ---- Bước 3: Copy version.json ----
    std::string srcVerJson = joinPath(extractDir, "version.json");
    if (!DownloadManager::FileExists(srcVerJson)) {
        if (st) st("ERROR: version.json missing in installer", 0.50f);
        return false;
    }
    std::error_code ec;
    fs::copy_file(srcVerJson, marker, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        if (st) st("ERROR: cannot copy version.json: " + ec.message(), 0.52f);
        return false;
    }

    // ---- Bước 4: Tìm universal jar ----
    std::string universalName = "forge-" + fv + "-universal.jar";
    std::string srcUniversal  = joinPath(extractDir, universalName);

    if (!DownloadManager::FileExists(srcUniversal)) {
        for (auto& entry : fs::directory_iterator(extractDir, ec)) {
            std::string fn = entry.path().filename().string();
            if (fn.find("universal") != std::string::npos &&
                fn.size() > 4 && fn.substr(fn.size()-4) == ".jar") {
                srcUniversal  = entry.path().string();
                universalName = fn;
                break;
            }
        }
    }
    if (!DownloadManager::FileExists(srcUniversal)) {
        if (st) st("ERROR: universal jar not found in installer", 0.55f);
        return false;
    }

    // ---- Bước 5: Copy universal jar → libraries ----
    std::string libDir = joinPath(mcDir,
        "libraries/net/minecraftforge/forge/" + fv);
    DownloadManager::EnsureDir(libDir);
    std::string libJar = joinPath(libDir, universalName);
    fs::copy_file(srcUniversal, libJar, fs::copy_options::overwrite_existing, ec);

    // ---- Bước 6: Copy universal jar → version jar ----
    std::string verJar = joinPath(verDir, outVersionId + ".jar");
    fs::copy_file(srcUniversal, verJar, fs::copy_options::overwrite_existing, ec);

    // ---- Bước 7: Inject library vào version.json ----
    if (st) st("Configuring Forge profile...", 0.70f);
    {
        std::ifstream fin(marker);
        json vj;
        try { fin >> vj; } catch (...) {
            if (st) st("ERROR: invalid version.json", 0.72f);
            return false;
        }
        fin.close();

        if (!vj.contains("libraries")) vj["libraries"] = json::array();

        bool hasForge = false;
        for (auto& lib : vj["libraries"]) {
            std::string name = lib.value("name", "");
            if (name.find("net.minecraftforge:forge:") == 0) { hasForge = true; break; }
        }

        if (!hasForge) {
            uint64_t sz = 0;
            { std::error_code e2; sz = fs::file_size(libJar, e2); }
            json forgeLib = {
                {"name", "net.minecraftforge:forge:" + fv},
                {"downloads", {
                    {"artifact", {
                        {"path", "net/minecraftforge/forge/" + fv + "/" + universalName},
                        {"url",  ""},
                        {"size", (int64_t)sz}
                    }}
                }}
            };
            vj["libraries"].push_back(forgeLib);
        }

        std::ofstream fout(marker);
        fout << vj.dump(2);
    }

    // ---- Bước 8: Download libraries bổ sung từ version.json (forge deps) ----
    if (st) st("Downloading Forge libraries...", 0.80f);
    {
        std::ifstream fin(marker);
        json vj;
        try { fin >> vj; } catch (...) { return false; }
        downloadAllLibraries(mcDir, vj, st, 0.80f, 0.95f);
    }

    if (st) st("Forge installed successfully", 1.f);
    return DownloadManager::FileExists(marker);
}

// ==================== OPTIFINE ====================
bool Minecraft::EnsureOptiFine(const std::string& mcDir, StatusFn st) {
    std::string modsDir = joinPath(mcDir, "mods");
    DownloadManager::EnsureDir(modsDir);
    std::string dst = joinPath(modsDir, "OptiFine_1.8.9_HD_U_M5.jar");
    if (DownloadManager::FileExists(dst)) return true;

    if (st) st("Downloading OptiFine...", 0.50f);

    // Thử mirror BMCLAPI trước, nếu fail thì thử optifine.net
    std::string mirror = "https://bmclapi2.bangbang93.com/optifine/1.8.9/HD_U_M5";
    if (!DownloadManager::Download(mirror, dst)) {
        std::string url = "https://optifine.net/adloadx?f=OptiFine_1.8.9_HD_U_M5.jar";
        if (!DownloadManager::Download(url, dst)) return false;
    }
    return DownloadManager::FileExists(dst);
}

// ==================== CLASSPATH ====================
static std::string buildClasspath(const std::string& mcDir, const json& vjson,
                                  const std::string& versionId)
{
    std::vector<std::string> parts;
    if (vjson.contains("libraries")) {
        for (auto& lib : vjson["libraries"]) {
            if (lib.contains("rules")) {
                bool allow = false;
                for (auto& r : lib["rules"]) {
                    bool osMatch = true;
                    if (r.contains("os")) {
                        std::string osName = r["os"].value("name","");
                        if (!osName.empty() && osName != "windows") osMatch = false;
                    }
                    if (osMatch) allow = (r.value("action","allow") == "allow");
                }
                if (!allow) continue;
            }
            if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                std::string rel = lib["downloads"]["artifact"].value("path","");
                if (!rel.empty()) parts.push_back(joinPath(mcDir, "libraries/" + rel));
            }
        }
    }
    // Version jar (Forge universal hoặc vanilla client)
    std::string cj = joinPath(mcDir, "versions/" + versionId + "/" + versionId + ".jar");
    if (!DownloadManager::FileExists(cj))
        cj = joinPath(mcDir, "versions/1.8.9/1.8.9.jar");
    parts.push_back(cj);

    std::string cp;
    for (size_t i = 0; i < parts.size(); ++i) {
        if (i) cp += ';';
        cp += parts[i];
    }
    return cp;
}

// ==================== LAUNCH ====================
LaunchResult Minecraft::Launch(const std::string& mcDir,
                               const std::string& javaPath,
                               const std::string& versionId,
                               const std::string& username,
                               int ramMB,
                               int winW, int winH)
{
    LaunchResult r;
    std::string vjsonPath = joinPath(mcDir, "versions/" + versionId + "/" + versionId + ".json");
    std::ifstream f(vjsonPath);
    if (!f) { r.error = "Version JSON not found: " + vjsonPath; return r; }
    json vjson; try { f >> vjson; } catch (...) { r.error = "Invalid version JSON"; return r; }

    std::string mainClass = vjson.value("mainClass", "net.minecraft.client.main.Main");
    std::string cp = buildClasspath(mcDir, vjson, versionId);

    std::string natives = joinPath(mcDir, "versions/" + versionId + "/natives");
    DownloadManager::EnsureDir(natives);

    // Extract native .dll từ các jar natives
    for (auto& lib : vjson["libraries"]) {
        if (lib.contains("natives")) {
            std::string cls = lib["natives"].value("windows","");
            if (!cls.empty() && lib["downloads"].contains("classifiers") &&
                lib["downloads"]["classifiers"].contains(cls))
            {
                std::string rel = lib["downloads"]["classifiers"][cls].value("path","");
                std::string jar = joinPath(mcDir, "libraries/" + rel);
                if (DownloadManager::FileExists(jar)) {
                    std::string ps =
                        "powershell -NoProfile -Command \"Add-Type -A System.IO.Compression.FileSystem;"
                        "[IO.Compression.ZipFile]::ExtractToDirectory('" + jar + "','" + natives + "')\"";
                    std::string log = joinPath(Settings::I().appDataDir(), "natives.log");
                    RunProcessCapture(ps, "", log);
                }
            }
        }
    }

    std::string assetsDir = joinPath(mcDir, "assets");
    std::string assetIndex = vjson.value("assets", "1.8");
    std::string gameDir = mcDir;

    std::stringstream cmd;
    cmd << "\"" << javaPath << "\" ";
    cmd << "-Xmx" << ramMB << "M ";
    cmd << "-Djava.library.path=\"" << natives << "\" ";
    cmd << "-Dminecraft.launcher.brand=RavenXD -Dminecraft.launcher.version=1.0.0 ";
    cmd << "-cp \"" << cp << "\" ";
    cmd << mainClass << " ";
    cmd << "--username " << username << " ";
    cmd << "--version " << versionId << " ";
    cmd << "--gameDir \"" << gameDir << "\" ";
    cmd << "--assetsDir \"" << assetsDir << "\" ";
    cmd << "--assetIndex " << assetIndex << " ";
    cmd << "--uuid 00000000000000000000000000000000 ";
    cmd << "--accessToken 0 ";
    cmd << "--userType legacy ";
    cmd << "--versionType RavenXD ";
    cmd << "--width " << winW << " --height " << winH;

    std::string cmdline = cmd.str();

    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<char> cmdbuf(cmdline.begin(), cmdline.end()); cmdbuf.push_back(0);

    BOOL ok = CreateProcessA(nullptr, cmdbuf.data(), nullptr, nullptr, FALSE, 0,
                             nullptr, mcDir.c_str(), &si, &pi);
    if (!ok) { r.error = "Failed to start Minecraft"; return r; }
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    r.ok = true;
    return r;
}
