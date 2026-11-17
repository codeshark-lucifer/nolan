#include "actions.hpp"
#include "config.hpp"
#include "engine/engine.h"
#include "launcher/forge_installer.hpp"
#include "launcher/launcher.hpp"
#include "launcher/mod_manager.hpp"
#include "platform_utils.hpp"
#include "state.hpp"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>

namespace fs = std::filesystem;

namespace
{
    void ApplyFetchedVersionsLocked(std::vector<VersionEntry> &&fetched)
    {
        std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
        std::string desiredId = g_Launcher.selectedVersionId;
        if (desiredId.empty() &&
            g_Launcher.selectedVersion >= 0 &&
            g_Launcher.selectedVersion < static_cast<int>(g_Launcher.versions.size()))
        {
            desiredId = g_Launcher.versions[static_cast<size_t>(g_Launcher.selectedVersion)].id;
        }

        g_Launcher.versions = std::move(fetched);

        int newIndex = 0;
        if (!desiredId.empty())
        {
            for (int i = 0; i < static_cast<int>(g_Launcher.versions.size()); ++i)
            {
                if (g_Launcher.versions[static_cast<size_t>(i)].id == desiredId)
                {
                    newIndex = i;
                    break;
                }
            }
        }
        g_Launcher.selectedVersion = newIndex;
        if (!g_Launcher.versions.empty())
            g_Launcher.selectedVersionId = g_Launcher.versions[static_cast<size_t>(newIndex)].id;
    }
} // namespace

void RefreshVersionsAsync()
{
    // Load local + cached manifest immediately for instant UI display
    {
        std::string gameDir = g_Launcher.GetGameDir();
        bool snapshots = g_Launcher.showSnapshots;
        auto cached = MinecraftLauncher::LoadCachedVersions(gameDir, snapshots);
        if (!cached.empty())
            ApplyFetchedVersionsLocked(std::move(cached));
    }

    if (g_Launcher.isBusy.exchange(true))
        return;

    g_Launcher.SetStatus("Syncing version manifest...");
    std::string gameDir = g_Launcher.GetGameDir();
    bool snapshots = g_Launcher.showSnapshots;

    std::thread([gameDir, snapshots]() {
        g_Launcher.AppendLog("[Launcher] Fetching Minecraft versions...");
        auto fetched = MinecraftLauncher::FetchVersions(gameDir, snapshots);
        size_t count = fetched.size();
        ApplyFetchedVersionsLocked(std::move(fetched));
        g_Launcher.AppendLog("[Launcher] Loaded " + std::to_string(count) + " versions.");
        g_Launcher.SetStatus("Ready");
        g_Launcher.isBusy = false;
    }).detach();
}

void RefreshForgePromotionsAsync()
{
    std::string gameDir = g_Launcher.GetGameDir();
    {
        auto cached = ForgeInstaller::LoadCachedForgePromotions(gameDir);
        if (!cached.empty())
        {
            std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
            if (g_Launcher.forgePromos.empty())
                g_Launcher.forgePromos = std::move(cached);
        }
    }

    std::thread([gameDir]() {
        auto promos = ForgeInstaller::FetchForgePromotions(gameDir);
        if (!promos.empty())
        {
            std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
            g_Launcher.forgePromos = std::move(promos);
            if (g_Launcher.selectedForgePromoIndex >= static_cast<int>(g_Launcher.forgePromos.size()))
                g_Launcher.selectedForgePromoIndex = 0;
        }
    }).detach();
}

void LaunchMinecraftAsync(const std::string &versionId)
{
    if (versionId.empty())
        return;
    if (g_Launcher.isBusy.exchange(true))
        return;

    g_Launcher.SetStatus("Preparing " + versionId + "...");
    g_Launcher.downloadProgress = 0.0f;
    g_Launcher.AppendLog("[Launcher] Preparing launch for " + versionId);

    LaunchOptions opts;
    opts.baseGameDir = g_Launcher.GetGameDir();
    {
        VersionEntry targetVer;
        {
            std::lock_guard<std::mutex> lock(g_Launcher.dataMutex);
            for (const auto &v : g_Launcher.versions)
            {
                if (v.id == versionId)
                {
                    targetVer = v;
                    break;
                }
            }
        }
        if (g_Launcher.isolateModdedDirs && targetVer.isModded)
            opts.instanceGameDir = (fs::path(opts.baseGameDir) / "instances" / versionId).string();
        else
            opts.instanceGameDir = opts.baseGameDir;
    }
    opts.customJavaPath = g_Launcher.customJavaPath;
    opts.customJvmArgs = g_Launcher.customJvmArgs;
    opts.ramGB = g_Launcher.ramAllocation;
    opts.captureLogs = g_Launcher.captureGameLogs;

    PlayerProfile targetProfile;
    if (g_Launcher.isOnlineMode && g_Launcher.isAuthenticated)
        targetProfile = g_Launcher.profile;
    else
        targetProfile = MinecraftLauncher::CreateOfflineProfile(g_Launcher.username);

    bool closeAfter = g_Launcher.closeAfterLaunch;
    bool snapshots = g_Launcher.showSnapshots;

    std::thread([versionId, opts, targetProfile, closeAfter, snapshots]() {
        bool launched = MinecraftLauncher::LaunchGame(
            targetProfile,
            versionId,
            opts,
            g_Launcher.downloadProgress,
            [](const std::string &status) { g_Launcher.SetStatus(status); },
            [](const std::string &line) { g_Launcher.AppendLog(line); });

        if (launched)
        {
            g_Launcher.SetStatus("Launched " + versionId + "!");
            g_Launcher.AppendLog("[Launcher] Minecraft " + versionId + " launched successfully.");
            auto refreshed = MinecraftLauncher::LoadCachedVersions(opts.baseGameDir, snapshots);
            ApplyFetchedVersionsLocked(std::move(refreshed));
            g_Launcher.SelectVersionById(versionId);
            SaveConfig();

            if (closeAfter)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(800));
                std::exit(0);
            }
        }
        else
        {
            g_Launcher.SetStatus("Failed to launch " + versionId + " (see Console Log).");
            g_Launcher.AppendLog("[Launcher] ERROR: Launch failed for " + versionId);
        }

        g_Launcher.isBusy = false;
    }).detach();
}

void InstallForgeOnlineAsync(const std::string &mcVersion, const std::string &forgeVersion)
{
    if (mcVersion.empty() || forgeVersion.empty())
        return;
    if (g_Launcher.isBusy.exchange(true))
        return;

    g_Launcher.downloadProgress = 0.0f;
    g_Launcher.SetStatus("Installing Forge " + mcVersion + "-" + forgeVersion + "...");
    g_Launcher.AppendLog("[Forge] Starting Forge installation: MC " + mcVersion + " / Forge " + forgeVersion);

    std::string gameDir = g_Launcher.GetGameDir();
    std::string customJava = g_Launcher.customJavaPath;
    bool snapshots = g_Launcher.showSnapshots;

    std::thread([mcVersion, forgeVersion, gameDir, customJava, snapshots]() {
        std::string installedId;
        bool ok = ForgeInstaller::DownloadAndInstallForge(
            mcVersion,
            forgeVersion,
            gameDir,
            customJava,
            g_Launcher.downloadProgress,
            [](const std::string &status) { g_Launcher.SetStatus(status); },
            [](const std::string &line) { g_Launcher.AppendLog(line); },
            installedId);

        if (ok && !installedId.empty())
        {
            auto refreshed = MinecraftLauncher::LoadCachedVersions(gameDir, snapshots);
            ApplyFetchedVersionsLocked(std::move(refreshed));
            g_Launcher.SelectVersionById(installedId);
            SaveConfig();
            g_Launcher.SetStatus("Forge ready: " + installedId);
        }
        g_Launcher.isBusy = false;
    }).detach();
}

void InstallForgeFromJarAsync(const std::string &installerJarPath)
{
    if (installerJarPath.empty())
        return;
    if (g_Launcher.isBusy.exchange(true))
        return;

    g_Launcher.downloadProgress = 0.0f;
    g_Launcher.SetStatus("Installing Forge from .jar...");
    g_Launcher.AppendLog("[Forge] Installing from local jar: " + installerJarPath);

    std::string gameDir = g_Launcher.GetGameDir();
    std::string customJava = g_Launcher.customJavaPath;
    bool snapshots = g_Launcher.showSnapshots;

    std::thread([installerJarPath, gameDir, customJava, snapshots]() {
        std::string installedId;
        bool ok = ForgeInstaller::InstallForgeFromInstallerJar(
            installerJarPath,
            gameDir,
            customJava,
            g_Launcher.downloadProgress,
            [](const std::string &status) { g_Launcher.SetStatus(status); },
            [](const std::string &line) { g_Launcher.AppendLog(line); },
            installedId);

        if (ok && !installedId.empty())
        {
            auto refreshed = MinecraftLauncher::LoadCachedVersions(gameDir, snapshots);
            ApplyFetchedVersionsLocked(std::move(refreshed));
            g_Launcher.SelectVersionById(installedId);
            SaveConfig();
            g_Launcher.SetStatus("Forge ready: " + installedId);
        }
        g_Launcher.isBusy = false;
    }).detach();
}

void UninstallVersionAsync(const std::string &versionId)
{
    if (versionId.empty())
        return;
    if (g_Launcher.isBusy.exchange(true))
        return;

    std::string gameDir = g_Launcher.GetGameDir();
    bool snapshots = g_Launcher.showSnapshots;
    g_Launcher.SetStatus("Removing " + versionId + "...");

    std::thread([versionId, gameDir, snapshots]() {
        std::error_code ec;
        fs::path verDir = fs::path(gameDir) / "versions" / versionId;
        fs::remove_all(verDir, ec);

        auto refreshed = MinecraftLauncher::LoadCachedVersions(gameDir, snapshots);
        ApplyFetchedVersionsLocked(std::move(refreshed));
        g_Launcher.AppendLog("[Launcher] Removed local version " + versionId);
        g_Launcher.SetStatus("Removed " + versionId);
        g_Launcher.isBusy = false;
    }).detach();
}

void StartMSALoginAsync()
{
    if (g_Launcher.isBusy.exchange(true))
        return;

    g_Launcher.SetStatus("Requesting Microsoft Device Code...");

    std::thread([]() {
        if (MicrosoftAuth::RequestDeviceCode(g_Launcher.msaCode))
        {
            g_Launcher.showAuthModal = true;
            g_Launcher.cancelAuth = false;

            PlatformUtils::OpenUrlInBrowser(g_Launcher.msaCode.verificationUri);

            std::string authStatus;
            bool success = MicrosoftAuth::AuthenticateWithDeviceCode(
                g_Launcher.msaCode, g_Launcher.profile, g_Launcher.cancelAuth, authStatus);

            if (success)
            {
                g_Launcher.isAuthenticated = true;
                g_Launcher.isOnlineMode = true;
                std::strncpy(g_Launcher.username, g_Launcher.profile.username.c_str(), sizeof(g_Launcher.username) - 1);
                g_Launcher.username[sizeof(g_Launcher.username) - 1] = '\0';
                std::strncpy(g_Launcher.profileName, g_Launcher.profile.username.c_str(), sizeof(g_Launcher.profileName) - 1);
                g_Launcher.profileName[sizeof(g_Launcher.profileName) - 1] = '\0';
                std::strncpy(g_Launcher.accountType, "Microsoft Account", sizeof(g_Launcher.accountType) - 1);
                g_Launcher.accountType[sizeof(g_Launcher.accountType) - 1] = '\0';
                SaveConfig();
            }

            g_Launcher.SetStatus(authStatus);
            g_Launcher.showAuthModal = false;
        }
        else
        {
            g_Launcher.SetStatus("Failed to communicate with Microsoft Auth Server.");
        }

        g_Launcher.isBusy = false;
    }).detach();
}

void RefreshModsCache(bool forceRescan)
{
    std::string modsDir = g_Launcher.GetActiveModsDir();
    if (!forceRescan && !g_Launcher.modsNeedRescan && g_Launcher.lastScannedModsDir == modsDir)
        return;

    // Unload Vulkan textures for any mod icons that will no longer exist or be replaced
    std::vector<ModInfo> oldMods = g_Launcher.mods;
    auto updated = ModManager::ScanModsDirectory(modsDir, oldMods);

    // Build set of reused textureIds so we only unload removed ones
    std::vector<uint64_t> reusedIds;
    for (const auto &m : updated)
    {
        if (m.textureId != 0)
            reusedIds.push_back(m.textureId);
    }
    for (const auto &old : oldMods)
    {
        if (old.textureId != 0 &&
            std::find(reusedIds.begin(), reusedIds.end(), old.textureId) == reusedIds.end())
        {
            OLIA_ENGINE::UnloadUITexture(old.textureId);
        }
    }

    g_Launcher.mods = std::move(updated);
    g_Launcher.lastScannedModsDir = modsDir;
    g_Launcher.modsNeedRescan = false;
}

void ImportModJarsAndNotify(const std::vector<std::string> &paths)
{
    std::string modsDir = g_Launcher.GetActiveModsDir();
    int importedCount = 0;
    std::string lastErr;

    for (const auto &src : paths)
    {
        std::string dest;
        std::string err;
        if (ModManager::ImportModJar(src, modsDir, dest, err))
        {
            importedCount++;
            g_Launcher.AppendLog("[Mods] Imported mod: " + fs::path(dest).filename().string());
        }
        else
        {
            lastErr = err;
        }
    }

    if (importedCount > 0)
    {
        g_Launcher.modsNeedRescan = true;
        RefreshModsCache(true);
        g_Launcher.SetStatus("Added " + std::to_string(importedCount) + " mod .jar file(s) to mods/");
    }
    else if (!lastErr.empty())
    {
        g_Launcher.SetStatus(lastErr);
    }
}

void HandleDroppedFiles(const std::vector<std::string> &paths)
{
    if (paths.empty())
        return;

    std::vector<std::string> modJars;
    for (const auto &p : paths)
    {
        std::string ext = fs::path(p).extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        if (ext != ".jar")
            continue;

        JarKind kind = ModManager::DetectJarKind(p);
        if (kind == JarKind::ForgeInstaller)
        {
            g_Launcher.pendingDroppedInstallerJar = p;
            g_Launcher.pendingDroppedInspection = ForgeInstaller::InspectForgeInstallerJar(p);
            g_Launcher.showDropInstallPrompt = true;
        }
        else
        {
            modJars.push_back(p);
        }
    }

    if (!modJars.empty())
    {
        ImportModJarsAndNotify(modJars);
        g_Launcher.currentPage = 2; // Switch to Mods tab so user immediately sees imported mods
    }
}
