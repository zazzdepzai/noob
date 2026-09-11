#include "DownloadManager.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <filesystem>
#include <vector>
#include <chrono>
#include <bcrypt.h>
#pragma comment(lib, "bcrypt.lib")

namespace fs = std::filesystem;

static std::wstring widen(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring w(n, 0);
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), w.data(), n);
    return w;
}

bool DownloadManager::EnsureDir(const std::string& path) {
    std::error_code ec; fs::create_directories(path, ec); return !ec;
}
bool DownloadManager::FileExists(const std::string& p) {
    std::error_code ec; return fs::exists(p, ec) && fs::is_regular_file(p, ec);
}
uint64_t DownloadManager::FileSize(const std::string& p) {
    std::error_code ec; auto s = fs::file_size(p, ec); return ec ? 0 : s;
}

bool DownloadManager::DownloadToString(const std::string& url, std::string& out) {
    std::wstring wurl = widen(url);
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

    HINTERNET ses = WinHttpOpen(L"RavenXD/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return false;

    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return false; }

    // follow redirects
    DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &opt, sizeof(opt));

    bool ok = false;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr)) {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status >= 200 && status < 300) {
            out.clear();
            DWORD avail = 0;
            do {
                avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) break;
                if (avail == 0) { ok = true; break; }
                size_t old = out.size();
                out.resize(old + avail);
                DWORD read = 0;
                if (!WinHttpReadData(req, out.data() + old, avail, &read)) { out.resize(old); break; }
                out.resize(old + read);
            } while (avail > 0);
        }
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
    return ok;
}

bool DownloadManager::Download(const std::string& url,
                               const std::string& outPath,
                               ProgressFn progress,
                               std::atomic<bool>* cancel)
{
    std::wstring wurl = widen(url);
    URL_COMPONENTS uc{}; uc.dwStructSize = sizeof(uc);
    wchar_t host[256]{}, path[2048]{};
    uc.lpszHostName = host; uc.dwHostNameLength = 255;
    uc.lpszUrlPath  = path; uc.dwUrlPathLength  = 2047;
    if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &uc)) return false;

    HINTERNET ses = WinHttpOpen(L"RavenXD/1.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) return false;

    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    if (!con) { WinHttpCloseHandle(ses); return false; }

    DWORD flags = (uc.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET req = WinHttpOpenRequest(con, L"GET", path, nullptr,
                                       WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!req) { WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return false; }

    DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
    WinHttpSetOption(req, WINHTTP_OPTION_REDIRECT_POLICY, &opt, sizeof(opt));

    bool ok = false;
    if (WinHttpSendRequest(req, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) &&
        WinHttpReceiveResponse(req, nullptr))
    {
        DWORD status = 0, sz = sizeof(status);
        WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                            WINHTTP_HEADER_NAME_BY_INDEX, &status, &sz, WINHTTP_NO_HEADER_INDEX);
        if (status >= 200 && status < 300) {
            wchar_t clen[64]{}; DWORD clenSz = sizeof(clen);
            uint64_t total = 0;
            if (WinHttpQueryHeaders(req, WINHTTP_QUERY_CONTENT_LENGTH,
                                    WINHTTP_HEADER_NAME_BY_INDEX, clen, &clenSz,
                                    WINHTTP_NO_HEADER_INDEX)) {
                total = _wcstoui64(clen, nullptr, 10);
            }
            EnsureDir(fs::path(outPath).parent_path().string());

            // temp file then rename
            std::string tmp = outPath + ".part";
            std::ofstream f(tmp, std::ios::binary);
            if (!f) { WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses); return false; }

            uint64_t got = 0;
            auto t0 = std::chrono::steady_clock::now();
            DWORD avail = 0;
            bool downloadOk = false;
            do {
                if (cancel && cancel->load()) break;
                avail = 0;
                if (!WinHttpQueryDataAvailable(req, &avail)) break;
                if (avail == 0) { downloadOk = true; break; }
                std::vector<char> buf(avail);
                DWORD read = 0;
                if (!WinHttpReadData(req, buf.data(), avail, &read)) break;
                f.write(buf.data(), read);
                got += read;
                if (progress) {
                    auto t1 = std::chrono::steady_clock::now();
                    double sec = std::chrono::duration<double>(t1 - t0).count();
                    double bps = sec > 0.0 ? double(got) / sec : 0.0;
                    progress(got, total, bps);
                }
            } while (avail > 0);
            f.close();
            if (downloadOk) {
                std::error_code ec;
                fs::remove(outPath, ec);
                fs::rename(tmp, outPath, ec);
                ok = !ec;
            } else {
                std::error_code ec; fs::remove(tmp, ec);
            }
        }
    }
    WinHttpCloseHandle(req); WinHttpCloseHandle(con); WinHttpCloseHandle(ses);
    return ok;
}

// -------- SHA1 (BCrypt) --------
std::string DownloadManager::Sha1OfString(const std::string& s) {
    BCRYPT_ALG_HANDLE alg = nullptr;
    BCRYPT_HASH_HANDLE h = nullptr;
    std::string hex;
    if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA1_ALGORITHM, nullptr, 0) != 0) return hex;
    DWORD objLen = 0, cb = 0, hashLen = 0;
    BCryptGetProperty(alg, BCRYPT_OBJECT_LENGTH, (PUCHAR)&objLen, sizeof(objLen), &cb, 0);
    BCryptGetProperty(alg, BCRYPT_HASH_LENGTH, (PUCHAR)&hashLen, sizeof(hashLen), &cb, 0);
    std::vector<UCHAR> obj(objLen), hash(hashLen);
    if (BCryptCreateHash(alg, &h, obj.data(), objLen, nullptr, 0, 0) == 0) {
        BCryptHashData(h, (PUCHAR)s.data(), (ULONG)s.size(), 0);
        BCryptFinishHash(h, hash.data(), hashLen, 0);
        BCryptDestroyHash(h);
        static const char* H = "0123456789abcdef";
        hex.resize(hashLen * 2);
        for (DWORD i = 0; i < hashLen; ++i) {
            hex[i*2]   = H[(hash[i] >> 4) & 0xF];
            hex[i*2+1] = H[hash[i] & 0xF];
        }
    }
    BCryptCloseAlgorithmProvider(alg, 0);
    return hex;
}

std::string DownloadManager::Sha1OfFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return {};
    std::string data((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    return Sha1OfString(data);
}
