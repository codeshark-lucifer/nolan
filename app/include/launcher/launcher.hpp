#pragma once

// 1. Winsock2 & URLMon MUST be included before Windows.h
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <urlmon.h>

#pragma comment(lib, "urlmon.lib")

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <filesystem>
#include <sstream>
#include <algorithm>
#include <set>
#include <thread>
#include <atomic>
#include <future>

#include <httplib.h>
#include <nlohmann/json.hpp>

namespace fs = std::filesystem;
using json = nlohmann::json;

// --- Data Structures ---

struct PlayerProfile
{
    std::string username = "Steve";
    std::string uuid = "00000000-0000-0000-0000-000000000000";
    std::string accessToken = "0";
    std::string userType = "legacy";
    bool isOnline = false;
};

struct VersionEntry
{
    std::string id;
    std::string type;
    std::string url;
    bool isDownloaded = false;
};

// --- Helper Utilities ---

inline std::string Quote(const std::string &value)
{
    return "\"" + value + "\"";
}

inline std::string NormalizePath(std::string path)
{
    std::replace(path.begin(), path.end(), '\\', '/');
    return path;
}

inline std::string ReplaceToken(std::string str, const std::string &token, const std::string &replacement)
{
    size_t pos = 0;
    while ((pos = str.find(token, pos)) != std::string::npos) {
        str.replace(pos, token.length(), replacement);
        pos += replacement.length();
    }
    return str;
}

inline bool ParseUrl(const std::string &url, std::string &host, std::string &path)
{
    std::string s = url;
    if (s.rfind("https://", 0) == 0) s = s.substr(8);
    else if (s.rfind("http://", 0) == 0) s = s.substr(7);

    size_t pos = s.find('/');
    if (pos == std::string::npos) {
        host = s;
        path = "/";
    } else {
        host = s.substr(0, pos);
        path = s.substr(pos);
    }
    return true;
}

// Win32 SHA-1 Hash Calculation for File Integrity Verification
inline std::string CalculateSHA1(const std::string &filepath)
{
    HANDLE hFile = CreateFileA(filepath.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return "";

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[20] = {0};
    DWORD cbHash = 20;

    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash)) {
            BYTE buffer[8192];
            DWORD bytesRead = 0;
            while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, NULL) && bytesRead > 0) {
                CryptHashData(hHash, buffer, bytesRead, 0);
            }
            CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    CloseHandle(hFile);

    char hashStr[41];
    snprintf(hashStr, sizeof(hashStr),
             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             rgbHash[0], rgbHash[1], rgbHash[2], rgbHash[3], rgbHash[4],
             rgbHash[5], rgbHash[6], rgbHash[7], rgbHash[8], rgbHash[9],
             rgbHash[10], rgbHash[11], rgbHash[12], rgbHash[13], rgbHash[14],
             rgbHash[15], rgbHash[16], rgbHash[17], rgbHash[18], rgbHash[19]);

    return std::string(hashStr);
}

// Verify file integrity using size and optional SHA-1 hash
inline bool IsFileValid(const std::string &filepath, size_t expectedSize = 0, const std::string &expectedHash = "")
{
    if (!fs::exists(filepath)) return false;
    if (expectedSize > 0 && fs::file_size(filepath) != expectedSize) return false;
    
    if (!expectedHash.empty()) {
        std::string actualHash = CalculateSHA1(filepath);
        std::string expectedLower = expectedHash;
        std::transform(expectedLower.begin(), expectedLower.end(), expectedLower.begin(), ::tolower);
        if (actualHash != expectedLower) return false;
    }

    return true;
}

// Downloader with Win32 URLDownloadToFileA and C++ httplib fallback
inline bool DownloadFile(const std::string &url, const std::string &savePath)
{
    std::string normPath = NormalizePath(savePath);
    fs::create_directories(fs::path(normPath).parent_path());

    // 1. Win32 Native API (Handles HTTPS & 301/302 redirects natively)
    HRESULT hr = URLDownloadToFileA(NULL, url.c_str(), normPath.c_str(), 0, NULL);
    if (SUCCEEDED(hr) && fs::exists(normPath) && fs::file_size(normPath) > 0) {
        return true;
    }

    // 2. Windows system curl.exe with -L
    std::string cmd = "curl.exe -s -f -L --create-dirs -o \"" + normPath + "\" \"" + url + "\"";
    int ret = system(cmd.c_str());
    if (ret == 0 && fs::exists(normPath) && fs::file_size(normPath) > 0) {
        return true;
    }

    // 3. C++ httplib fallback
    std::string host, path;
    if (ParseUrl(url, host, path)) {
        httplib::SSLClient cli(host);
        cli.enable_server_certificate_verification(false);
        cli.set_follow_location(true);
        cli.set_connection_timeout(5, 0);

        httplib::Headers headers = {
            { "User-Agent", "MinecraftLauncher/1.0 (Windows NT 10.0; Win64; x64)" }
        };

        auto res = cli.Get(path.c_str(), headers);
        if (res && (res->status == 301 || res->status == 302) && res->has_header("Location")) {
            std::string redirectUrl = res->get_header_value("Location");
            return DownloadFile(redirectUrl, normPath);
        }
        if (res && res->status == 200) {
            std::ofstream out(normPath, std::ios::binary);
            if (out.is_open()) {
                out.write(res->body.c_str(), res->body.size());
                out.close();
                return true;
            }
        }
    }

    return false;
}

// Parallel Multi-Threaded Asset Downloader (8 Parallel Worker Threads)
inline void DownloadAssetObjectsParallel(const std::string &gameDir, const json &assetData, std::atomic<float> &progressOut)
{
    if (!assetData.contains("objects")) return;

    struct Task {
        std::string url;
        std::string path;
        size_t expectedSize;
        std::string expectedHash;
    };

    std::vector<Task> tasks;

    for (auto& [key, value] : assetData["objects"].items()) {
        std::string hash = value["hash"].get<std::string>();
        std::string prefix = hash.substr(0, 2);
        std::string objPath = gameDir + "\\assets\\objects\\" + prefix + "\\" + hash;
        size_t expectedSize = value.value("size", 0);

        // Fast Verification: Check file existence and size
        if (IsFileValid(objPath, expectedSize)) {
            continue;
        }

        std::string objUrl = "https://resources.download.minecraft.net/" + prefix + "/" + hash;
        tasks.push_back({objUrl, objPath, expectedSize, hash});
    }

    if (tasks.empty()) {
        progressOut = 1.0f;
        return;
    }

    std::cout << "[Launcher] Downloading " << tasks.size() << " missing/corrupted assets using parallel threads...\n";

    std::atomic<size_t> completedTasks = 0;
    std::atomic<size_t> taskIndex = 0;
    size_t totalTasks = tasks.size();

    size_t numThreads = std::min<size_t>(8, std::thread::hardware_concurrency());
    std::vector<std::thread> workers;

    for (size_t t = 0; t < numThreads; ++t) {
        workers.emplace_back([&]() {
            while (true) {
                size_t idx = taskIndex.fetch_add(1);
                if (idx >= totalTasks) break;

                const auto& task = tasks[idx];
                DownloadFile(task.url, task.path);

                size_t done = completedTasks.fetch_add(1) + 1;
                progressOut = (float)done / (float)totalTasks;
            }
        });
    }

    for (auto &w : workers) {
        if (w.joinable()) w.join();
    }

    progressOut = 1.0f;
    std::cout << "[Launcher] Asset synchronization completed successfully!\n";
}

inline std::string GenerateOfflineUUID(const std::string &username)
{
    std::string input = "OfflinePlayer:" + username;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[16] = {0};
    DWORD cbHash = 16;

    if (CryptAcquireContextA(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash)) {
            CryptHashData(hHash, (BYTE *)input.c_str(), (DWORD)input.length(), 0);
            CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    rgbHash[6] = (rgbHash[6] & 0x0f) | 0x30;
    rgbHash[8] = (rgbHash[8] & 0x3f) | 0x80;

    char uuidStr[37];
    snprintf(uuidStr, sizeof(uuidStr),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             rgbHash[0], rgbHash[1], rgbHash[2], rgbHash[3],
             rgbHash[4], rgbHash[5],
             rgbHash[6], rgbHash[7],
             rgbHash[8], rgbHash[9],
             rgbHash[10], rgbHash[11], rgbHash[12], rgbHash[13], rgbHash[14], rgbHash[15]);

    return std::string(uuidStr);
}

inline bool IsAllowedForWindowsX64(const std::string &path)
{
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    if (lower.find("linux") != std::string::npos) return false;
    if (lower.find("macos") != std::string::npos) return false;
    if (lower.find("osx") != std::string::npos) return false;
    if (lower.find("windows-x86") != std::string::npos) return false;
    if (lower.find("windows-arm64") != std::string::npos) return false;

    return true;
}

inline bool ShouldIncludeLibrary(const json &lib)
{
    if (lib.contains("rules")) {
        bool allowed = false;
        for (const auto &rule : lib["rules"]) {
            std::string action = rule.value("action", "");
            bool osMatches = true;

            if (rule.contains("os")) {
                std::string osName = rule["os"].value("name", "");
                if (osName != "windows") {
                    osMatches = false;
                }
            }

            if (action == "allow" && osMatches) allowed = true;
            if (action == "disallow" && osMatches) allowed = false;
        }
        if (!allowed) return false;
    }

    if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
        std::string libPath = lib["downloads"]["artifact"]["path"].get<std::string>();
        if (!IsAllowedForWindowsX64(libPath)) return false;
    }

    return true;
}

inline void ExtractNativeJar(const std::string &jarPath, const std::string &outputDir)
{
    if (!fs::exists(jarPath)) return;
    if (!IsAllowedForWindowsX64(jarPath)) return;

    fs::create_directories(outputDir);
    std::string cmd = "powershell -Command \"Expand-Archive -Path '" + jarPath + "' -DestinationPath '" + outputDir + "' -Force\" > NUL 2>&1";
    system(cmd.c_str());
}

// --- Main Launcher Class ---

class MinecraftLauncher
{
public:
    static std::vector<std::string> GetInstalledVersions()
    {
        std::vector<std::string> installed;
        char *appDataEnv = std::getenv("APPDATA");
        if (!appDataEnv) return installed;

        std::string verDir = std::string(appDataEnv) + "\\.minecraft\\versions";
        if (fs::exists(verDir)) {
            for (const auto &entry : fs::directory_iterator(verDir)) {
                if (entry.is_directory()) {
                    std::string verName = entry.path().filename().string();
                    std::string jarFile = entry.path().string() + "\\" + verName + ".jar";
                    std::string jsonFile = entry.path().string() + "\\" + verName + ".json";
                    if (fs::exists(jarFile) || fs::exists(jsonFile)) {
                        installed.push_back(verName);
                    }
                }
            }
        }
        return installed;
    }

    static std::vector<VersionEntry> FetchVersions()
    {
        std::vector<VersionEntry> versions;
        std::vector<std::string> localVersions = GetInstalledVersions();
        std::set<std::string> localSet(localVersions.begin(), localVersions.end());

        httplib::SSLClient cli("launchermeta.mojang.com");
        cli.enable_server_certificate_verification(false);

        auto res = cli.Get("/mc/game/version_manifest_v2.json");
        if (res && res->status == 200) {
            json data = json::parse(res->body);
            for (const auto &ver : data["versions"]) {
                if (ver["type"] == "release") {
                    std::string verId = ver["id"].get<std::string>();
                    bool downloaded = localSet.count(verId) > 0;
                    versions.push_back({
                        verId,
                        ver["type"].get<std::string>(),
                        ver["url"].get<std::string>(),
                        downloaded
                    });
                }
            }
        } else {
            for (const auto &localVer : localVersions) {
                versions.push_back({localVer, "release", "", true});
            }
        }

        return versions;
    }

    static PlayerProfile CreateOfflineProfile(const std::string &username)
    {
        PlayerProfile profile;
        profile.username = username;
        profile.uuid = GenerateOfflineUUID(username);
        profile.accessToken = "0";
        profile.userType = "legacy";
        profile.isOnline = false;
        return profile;
    }

    static bool EnsureVersionDownloaded(const std::string &version, json &outVerData, std::atomic<float> &progressOut, const std::string &versionUrl = "")
    {
        char *appDataEnv = std::getenv("APPDATA");
        if (!appDataEnv) return false;

        std::string gameDir = std::string(appDataEnv) + "\\.minecraft";
        std::string verDir = gameDir + "\\versions\\" + version;
        std::string jsonPath = verDir + "\\" + version + ".json";
        std::string jarPath = verDir + "\\" + version + ".jar";
        std::string nativesDir = verDir + "\\natives";

        fs::create_directories(verDir);
        fs::create_directories(nativesDir);

        std::string urlToUse = versionUrl;
        if (urlToUse.empty()) {
            for (const auto &v : FetchVersions()) {
                if (v.id == version) {
                    urlToUse = v.url;
                    break;
                }
            }
        }

        // 1. Download Version JSON
        if (!fs::exists(jsonPath) && !urlToUse.empty()) {
            std::cout << "[Launcher] Downloading version manifest for " << version << "...\n";
            DownloadFile(urlToUse, jsonPath);
        }

        if (!fs::exists(jsonPath)) return false;

        std::ifstream f(jsonPath);
        outVerData = json::parse(f);
        f.close();

        // 2. Download Client JAR
        if (!fs::exists(jarPath)) {
            if (outVerData.contains("downloads") && outVerData["downloads"].contains("client")) {
                std::string clientUrl = outVerData["downloads"]["client"]["url"].get<std::string>();
                std::cout << "[Launcher] Downloading client.jar...\n";
                DownloadFile(clientUrl, jarPath);
            }
        }

        // 3. Download Libraries & Natives
        if (outVerData.contains("libraries")) {
            for (const auto &lib : outVerData["libraries"]) {
                if (!ShouldIncludeLibrary(lib)) continue;

                if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                    auto artifact = lib["downloads"]["artifact"];
                    std::string libPath = gameDir + "\\libraries\\" + artifact["path"].get<std::string>();
                    std::string libUrl = artifact["url"].get<std::string>();
                    size_t expSize = artifact.value("size", 0);
                    std::string expHash = artifact.value("sha1", "");

                    if (!IsFileValid(libPath, expSize, expHash)) {
                        DownloadFile(libUrl, libPath);
                    }

                    if (lib.contains("natives") || libPath.find("native") != std::string::npos) {
                        ExtractNativeJar(libPath, nativesDir);
                    }
                }
            }
        }

        // 4. Parallel Asset Object Downloader
        if (outVerData.contains("assetIndex")) {
            std::string assetId = outVerData["assetIndex"]["id"].get<std::string>();
            std::string assetUrl = outVerData["assetIndex"]["url"].get<std::string>();
            std::string assetIndexPath = gameDir + "\\assets\\indexes\\" + assetId + ".json";

            if (!fs::exists(assetIndexPath)) {
                std::cout << "[Launcher] Downloading Asset Index (" << assetId << ")...\n";
                DownloadFile(assetUrl, assetIndexPath);
            }

            if (fs::exists(assetIndexPath)) {
                std::ifstream af(assetIndexPath);
                if (af.is_open()) {
                    json assetData = json::parse(af);
                    af.close();

                    DownloadAssetObjectsParallel(gameDir, assetData, progressOut);
                }
            }
        }

        return true;
    }

    static std::string BuildClasspath(const std::string &gameDir, const std::string &version, const json &verData)
    {
        std::string classpath = "";
        std::string clientJar = gameDir + "\\versions\\" + version + "\\" + version + ".jar";

        if (verData.contains("libraries")) {
            for (const auto &lib : verData["libraries"]) {
                if (!ShouldIncludeLibrary(lib)) continue;

                if (lib.contains("downloads") && lib["downloads"].contains("artifact")) {
                    std::string libPath = gameDir + "\\libraries\\" + lib["downloads"]["artifact"]["path"].get<std::string>();
                    if (fs::exists(libPath)) {
                        classpath += libPath + ";";
                    }
                }
            }
        }

        classpath += clientJar;
        return classpath;
    }

    static bool LaunchGame(const PlayerProfile &profile, const std::string &version, int ramGB, std::atomic<float> &progressOut)
    {
        char *appDataEnv = std::getenv("APPDATA");
        if (!appDataEnv) return false;

        std::string gameDir = std::string(appDataEnv) + "\\.minecraft";
        std::string assetsDir = gameDir + "\\assets";
        std::string nativesDir = gameDir + "\\versions\\" + version + "\\natives";

        json verData;
        if (!EnsureVersionDownloaded(version, verData, progressOut)) {
            std::cerr << "[Launcher] Could not load or download version data.\n";
            return false;
        }

        std::string assetIndexId = version;
        if (verData.contains("assetIndex") && verData["assetIndex"].contains("id")) {
            assetIndexId = verData["assetIndex"]["id"].get<std::string>();
        }

        std::string classpath = BuildClasspath(gameDir, version, verData);

        std::string cmd = "java.exe";
        cmd += " -Xmx" + std::to_string(ramGB) + "G";
        cmd += " -Xss1M";
        cmd += " -Dfile.encoding=UTF-8";
        cmd += " -Duser.language=en";
        cmd += " -Djava.library.path=" + Quote(nativesDir);
        cmd += " -cp " + Quote(classpath);

        std::string mainClass = verData.value("mainClass", "net.minecraft.client.main.Main");
        cmd += " " + mainClass;

        cmd += " --username " + profile.username;
        cmd += " --version " + version;
        cmd += " --gameDir " + Quote(gameDir);
        cmd += " --assetsDir " + Quote(assetsDir);
        cmd += " --assetIndex " + assetIndexId;
        cmd += " --uuid " + profile.uuid;
        cmd += " --accessToken " + profile.accessToken;
        cmd += " --userType " + profile.userType;
        cmd += " --versionType release";

        std::cout << "[Launcher] Spawning Process: " << cmd << "\n";

        STARTUPINFOA si = { sizeof(si) };
        PROCESS_INFORMATION pi;

        BOOL success = CreateProcessA(
            NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE,
            CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi
        );

        if (success) {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }

        return false;
    }
};