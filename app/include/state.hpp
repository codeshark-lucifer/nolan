#pragma once

#include <imgui.h>
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

#include "launcher/forge_installer.hpp"
#include "launcher/launcher.hpp"
#include "launcher/mod_manager.hpp"
#include "platform_utils.hpp"

extern ImFont *g_FontRegular;
extern ImFont *g_FontSmall;
extern ImFont *g_FontBold;
extern ImFont *g_FontTitle;
extern ImFont *g_FontHero;

struct LauncherTextures
{
    ImTextureID appIcon = 0;
    ImTextureID heroWall = 0;
    int heroWallWidth = 0;
    int heroWallHeight = 0;
    ImTextureID profileAvatar = 0;
};

extern LauncherTextures g_Textures;

struct LauncherData
{
    // Synchronization for background worker updates
    mutable std::mutex dataMutex;

    // Profile State
    char username[64] = "Steve";
    char profileName[64] = "Steve";
    char accountType[64] = "Nolan Account";
    bool isOnlineMode = false;
    PlayerProfile profile;

    // Navigation
    int currentPage = 0;  // 0 = Play, 1 = Installations, 2 = Mods, 3 = Settings
    int settingsPage = 0; // 0 = General, 1 = Java & Performance, 2 = Profile, 3 = Logs
    bool showConsoleDrawer = false;

    // General & Performance Settings
    bool closeAfterLaunch = false;
    bool checkForUpdates = true;
    bool useSystemLanguage = true;
    bool showSnapshots = false;
    bool isolateModdedDirs = false;
    bool powerSavingMode = true;
    bool vsyncEnabled = true;
    bool captureGameLogs = true;
    int ramAllocation = 4;
    int systemMemoryGB = 16;
    char minecraftDirectory[512] = "";
    char customJavaPath[512] = "";
    char customJvmArgs[512] = "";

    // Versions & Forge Data
    std::vector<VersionEntry> versions;
    int selectedVersion = 0;
    std::string selectedVersionId = "";
    std::vector<ForgePromotionEntry> forgePromos;

    // Mods Data (.jar)
    std::vector<ModInfo> mods;
    bool modsNeedRescan = true;
    std::string lastScannedModsDir = "";

    // Java Runtimes Cache
    std::vector<JavaRuntimeInfo> javaRuntimes;

    // Async Operations & Feedback
    std::atomic<bool> isBusy{false};
    std::atomic<float> downloadProgress{0.0f};
    std::string statusText = "Ready";
    std::vector<std::string> logs;

    // Microsoft Authentication
    MSADeviceCode msaCode;
    bool showAuthModal = false;
    std::atomic<bool> cancelAuth{false};
    bool isAuthenticated = false;

    // Modals & Confirmation Dialogs
    bool showUninstallDialog = false;
    std::string versionToUninstall = "";

    bool showForgeInstallerModal = false;
    int forgeModalTab = 0; // 0 = Download Official Forge, 1 = Local Installer .jar
    int selectedForgePromoIndex = 0;
    bool preferRecommendedForge = true;
    char localForgeJarPath[512] = "";
    ForgeJarInspection localForgeInspection;

    bool showDropInstallPrompt = false;
    std::string pendingDroppedInstallerJar;
    ForgeJarInspection pendingDroppedInspection;

    // Thread-safe helpers
    void SetStatus(const std::string &text);
    std::string GetStatus() const;
    void AppendLog(const std::string &line);
    std::vector<std::string> GetLogsSnapshot() const;
    void ClearLogs();

    std::string GetGameDir() const;
    std::string GetEffectiveGameDirForSelected() const;
    std::string GetActiveModsDir() const;
    VersionEntry GetSelectedVersionCopy() const;
    void SelectVersionById(const std::string &id);
};

extern LauncherData g_Launcher;

const char *GetSpinnerChar();
