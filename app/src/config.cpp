#include "config.hpp"
#include "engine/engine.h"
#include "state.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <nlohmann/json.hpp>

#include <cstring>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;
using json = nlohmann::json;

static std::string GetConfigFilePath()
{
    char path[MAX_PATH] = {0};
    DWORD length = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (length > 0)
    {
        fs::path exePath(path);
        return (exePath.parent_path() / "data.json").string();
    }
    return "data.json";
}

void SaveConfig()
{
    std::string filePath = GetConfigFilePath();
    std::ofstream file(filePath, std::ios::trunc);
    if (!file.is_open())
        return;

    json cfg = {
        {"username", g_Launcher.username},
        {"profileName", g_Launcher.profileName},
        {"accountType", g_Launcher.accountType},
        {"isOnlineMode", g_Launcher.isOnlineMode},
        {"closeAfterLaunch", g_Launcher.closeAfterLaunch},
        {"checkForUpdates", g_Launcher.checkForUpdates},
        {"useSystemLanguage", g_Launcher.useSystemLanguage},
        {"showSnapshots", g_Launcher.showSnapshots},
        {"isolateModdedDirs", g_Launcher.isolateModdedDirs},
        {"powerSavingMode", g_Launcher.powerSavingMode},
        {"vsyncEnabled", g_Launcher.vsyncEnabled},
        {"captureGameLogs", g_Launcher.captureGameLogs},
        {"ramAllocation", g_Launcher.ramAllocation},
        {"minecraftDirectory", g_Launcher.minecraftDirectory},
        {"customJavaPath", g_Launcher.customJavaPath},
        {"customJvmArgs", g_Launcher.customJvmArgs},
        {"selectedVersion", g_Launcher.selectedVersion},
        {"selectedVersionId", g_Launcher.selectedVersionId}
    };

    file << cfg.dump(2) << "\n";
}

void LoadConfig()
{
    std::string filePath = GetConfigFilePath();
    std::ifstream file(filePath);
    if (!file.is_open())
        return;

    json cfg = json::parse(file, nullptr, false);
    file.close();
    if (!cfg.is_object())
        return;

    auto copyStr = [&](const char *key, char *dst, size_t dstSize) {
        if (cfg.contains(key) && cfg[key].is_string())
        {
            std::string val = cfg[key].get<std::string>();
            if (!val.empty())
            {
                std::strncpy(dst, val.c_str(), dstSize - 1);
                dst[dstSize - 1] = '\0';
            }
        }
    };

    copyStr("username", g_Launcher.username, sizeof(g_Launcher.username));
    copyStr("profileName", g_Launcher.profileName, sizeof(g_Launcher.profileName));
    copyStr("accountType", g_Launcher.accountType, sizeof(g_Launcher.accountType));
    copyStr("minecraftDirectory", g_Launcher.minecraftDirectory, sizeof(g_Launcher.minecraftDirectory));
    copyStr("customJavaPath", g_Launcher.customJavaPath, sizeof(g_Launcher.customJavaPath));
    copyStr("customJvmArgs", g_Launcher.customJvmArgs, sizeof(g_Launcher.customJvmArgs));

    g_Launcher.isOnlineMode = cfg.value("isOnlineMode", g_Launcher.isOnlineMode);
    g_Launcher.closeAfterLaunch = cfg.value("closeAfterLaunch", g_Launcher.closeAfterLaunch);
    g_Launcher.checkForUpdates = cfg.value("checkForUpdates", g_Launcher.checkForUpdates);
    g_Launcher.useSystemLanguage = cfg.value("useSystemLanguage", g_Launcher.useSystemLanguage);
    g_Launcher.showSnapshots = cfg.value("showSnapshots", g_Launcher.showSnapshots);
    g_Launcher.isolateModdedDirs = cfg.value("isolateModdedDirs", g_Launcher.isolateModdedDirs);
    g_Launcher.powerSavingMode = cfg.value("powerSavingMode", g_Launcher.powerSavingMode);
    g_Launcher.vsyncEnabled = cfg.value("vsyncEnabled", g_Launcher.vsyncEnabled);
    g_Launcher.captureGameLogs = cfg.value("captureGameLogs", g_Launcher.captureGameLogs);

    g_Launcher.ramAllocation = cfg.value("ramAllocation", g_Launcher.ramAllocation);
    g_Launcher.selectedVersion = cfg.value("selectedVersion", g_Launcher.selectedVersion);
    g_Launcher.selectedVersionId = cfg.value("selectedVersionId", g_Launcher.selectedVersionId);

    OLIA_ENGINE::SetPowerSaving(g_Launcher.powerSavingMode);
    OLIA_ENGINE::SetVSync(g_Launcher.vsyncEnabled);
}
