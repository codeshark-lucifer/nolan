#pragma once

#include <string>
#include <vector>

void RefreshVersionsAsync();
void RefreshForgePromotionsAsync();
void LaunchMinecraftAsync(const std::string &versionId);
void InstallForgeOnlineAsync(const std::string &mcVersion, const std::string &forgeVersion);
void InstallForgeFromJarAsync(const std::string &installerJarPath);
void UninstallVersionAsync(const std::string &versionId);
void StartMSALoginAsync();

void RefreshModsCache(bool forceRescan = false);
void ImportModJarsAndNotify(const std::vector<std::string> &paths);
void HandleDroppedFiles(const std::vector<std::string> &paths);
