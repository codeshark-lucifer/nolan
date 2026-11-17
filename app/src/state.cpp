#include "state.hpp"

#include <filesystem>

namespace fs = std::filesystem;

ImFont *g_FontRegular = nullptr;
ImFont *g_FontSmall = nullptr;
ImFont *g_FontBold = nullptr;
ImFont *g_FontTitle = nullptr;
ImFont *g_FontHero = nullptr;

LauncherTextures g_Textures{};
LauncherData g_Launcher;

void LauncherData::SetStatus(const std::string &text)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    statusText = text;
}

std::string LauncherData::GetStatus() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return statusText;
}

void LauncherData::AppendLog(const std::string &line)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    logs.push_back(line);
    if (logs.size() > 600)
    {
        logs.erase(logs.begin(), logs.begin() + static_cast<ptrdiff_t>(logs.size() - 600));
    }
}

std::vector<std::string> LauncherData::GetLogsSnapshot() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    return logs;
}

void LauncherData::ClearLogs()
{
    std::lock_guard<std::mutex> lock(dataMutex);
    logs.clear();
}

std::string LauncherData::GetGameDir() const
{
    if (minecraftDirectory[0] != '\0')
        return std::string(minecraftDirectory);
    return MinecraftLauncher::GetDefaultMinecraftDir();
}

std::string LauncherData::GetEffectiveGameDirForSelected() const
{
    std::string base = GetGameDir();
    VersionEntry ver = GetSelectedVersionCopy();
    if (isolateModdedDirs && ver.isModded && !ver.id.empty())
    {
        return (fs::path(base) / "instances" / ver.id).string();
    }
    return base;
}

std::string LauncherData::GetActiveModsDir() const
{
    return (fs::path(GetEffectiveGameDirForSelected()) / "mods").string();
}

VersionEntry LauncherData::GetSelectedVersionCopy() const
{
    std::lock_guard<std::mutex> lock(dataMutex);
    if (versions.empty())
        return {};
    int idx = selectedVersion;
    if (idx < 0 || idx >= static_cast<int>(versions.size()))
        idx = 0;
    return versions[static_cast<size_t>(idx)];
}

void LauncherData::SelectVersionById(const std::string &id)
{
    std::lock_guard<std::mutex> lock(dataMutex);
    for (int i = 0; i < static_cast<int>(versions.size()); ++i)
    {
        if (versions[static_cast<size_t>(i)].id == id)
        {
            selectedVersion = i;
            selectedVersionId = id;
            modsNeedRescan = true;
            return;
        }
    }
}

const char *GetSpinnerChar()
{
    static const char *frames[] = {"|", "/", "-", "\\"};
    int idx = static_cast<int>(ImGui::GetTime() * 10.0) % 4;
    return frames[idx];
}
