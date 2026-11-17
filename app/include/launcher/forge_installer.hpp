#pragma once

#include "launcher/launcher.hpp"

#include <atomic>
#include <string>
#include <vector>

struct ForgePromotionEntry
{
    std::string mcVersion;
    std::string recommendedForge;
    std::string latestForge;
};

struct ForgeJarInspection
{
    bool valid = false;
    bool isLegacy = false;
    std::string mcVersion;
    std::string forgeVersion;
    std::string targetVersionId;
};

namespace ForgeInstaller
{
    std::vector<ForgePromotionEntry> LoadCachedForgePromotions(const std::string &gameDir);
    std::vector<ForgePromotionEntry> FetchForgePromotions(const std::string &gameDir);

    ForgeJarInspection InspectForgeInstallerJar(const std::string &jarPath);

    bool InstallForgeFromInstallerJar(
        const std::string &installerJarPath,
        const std::string &gameDir,
        const std::string &customJavaPath,
        std::atomic<float> &progressOut,
        const StatusCallback &statusCb,
        const LogCallback &logCb,
        std::string &outInstalledVersionId);

    bool DownloadAndInstallForge(
        const std::string &mcVersion,
        const std::string &forgeVersion,
        const std::string &gameDir,
        const std::string &customJavaPath,
        std::atomic<float> &progressOut,
        const StatusCallback &statusCb,
        const LogCallback &logCb,
        std::string &outInstalledVersionId);
} // namespace ForgeInstaller
