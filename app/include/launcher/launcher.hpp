#pragma once

#include <nlohmann/json.hpp>

#include <atomic>
#include <functional>
#include <string>
#include <vector>

using json = nlohmann::json;

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
    std::string type; // "release", "snapshot", "forge", "modded"
    std::string url;
    std::string inheritsFrom;
    std::string loaderName; // "", "Forge", "NeoForge", "Fabric"
    bool isDownloaded = false;
    bool isModded = false;
};

struct MSADeviceCode
{
    std::string userCode;
    std::string deviceCode;
    std::string verificationUri;
    int interval = 5;
    int expiresIn = 900;
};

struct LaunchOptions
{
    std::string baseGameDir;
    std::string instanceGameDir;
    std::string customJavaPath;
    std::string customJvmArgs;
    int ramGB = 4;
    bool captureLogs = true;
};

using StatusCallback = std::function<void(const std::string &)>;
using LogCallback = std::function<void(const std::string &)>;

class MicrosoftAuth
{
public:
    static bool RequestDeviceCode(MSADeviceCode &outCode);
    static bool AuthenticateWithDeviceCode(
        const MSADeviceCode &code,
        PlayerProfile &outProfile,
        std::atomic<bool> &cancelFlag,
        std::string &statusOut);
};

class MinecraftLauncher
{
public:
    static std::string GetDefaultMinecraftDir();
    static std::string CalculateSHA1(const std::string &filepath);
    static bool IsFileValid(const std::string &filepath, size_t expectedSize = 0, const std::string &expectedHash = "");
    static bool DownloadFile(const std::string &url, const std::string &savePath);
    static bool DownloadString(const std::string &url, std::string &outBody);

    static std::string MavenCoordToRelativePath(const std::string &coord);
    static std::string GenerateOfflineUUID(const std::string &username);
    static PlayerProfile CreateOfflineProfile(const std::string &username);

    static std::vector<VersionEntry> LoadCachedVersions(const std::string &gameDir, bool includeSnapshots = false);
    static std::vector<VersionEntry> FetchVersions(const std::string &gameDir, bool includeSnapshots = false);

    static bool ResolveVersionJson(
        const std::string &gameDir,
        const std::string &versionId,
        json &outMergedData,
        const std::string &versionUrl = "");

    static bool EnsureVersionDownloaded(
        const std::string &gameDir,
        const std::string &versionId,
        json &outResolvedData,
        std::atomic<float> &progressOut,
        const std::string &versionUrl = "",
        const StatusCallback &statusCb = nullptr,
        const LogCallback &logCb = nullptr);

    static std::string BuildClasspath(
        const std::string &gameDir,
        const std::string &versionId,
        const json &verData);

    static bool LaunchGame(
        const PlayerProfile &profile,
        const std::string &versionId,
        const LaunchOptions &options,
        std::atomic<float> &progressOut,
        const StatusCallback &statusCb = nullptr,
        const LogCallback &logCb = nullptr);
};
