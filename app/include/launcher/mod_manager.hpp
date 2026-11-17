#pragma once

#include <cstdint>
#include <string>
#include <vector>

enum class JarKind
{
    Unknown,
    ForgeInstaller,
    ForgeMod,
    NeoForgeMod,
    FabricMod
};

struct ModInfo
{
    std::string fileName;
    std::string filePath;
    bool enabled = true;

    JarKind kind = JarKind::Unknown;
    std::string modId;
    std::string displayName;
    std::string version;
    std::string authors;
    std::string description;
    std::string mcVersion;
    std::string loaderInfo;

    uint64_t fileSize = 0;
    int64_t lastWriteTicks = 0;

    std::vector<uint8_t> iconBytes;
    uint64_t textureId = 0; // Managed by UI thread
    bool textureAttempted = false;
};

namespace ModManager
{
    JarKind DetectJarKind(const std::string &jarPath, std::string *outLabel = nullptr);
    ModInfo InspectModJar(const std::string &filePath);

    std::vector<ModInfo> ScanModsDirectory(
        const std::string &modsDir,
        const std::vector<ModInfo> &previousCache = {});

    bool ImportModJar(
        const std::string &sourcePath,
        const std::string &modsDir,
        std::string &outDestPath,
        std::string &outError);

    bool SetModEnabled(ModInfo &mod, bool enable, std::string &outError);
    bool DeleteModFile(const ModInfo &mod, std::string &outError);
    std::string GetLoaderDisplayName(JarKind kind);
} // namespace ModManager
