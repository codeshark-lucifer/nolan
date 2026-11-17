#include "launcher/forge_installer.hpp"
#include "launcher/zip_utils.hpp"
#include "platform_utils.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <tuple>

namespace fs = std::filesystem;

namespace ForgeInstaller
{
    namespace
    {
        std::tuple<int, int, int, std::string> ParseMcVersionTuple(const std::string &v)
        {
            int major = 0, minor = 0, patch = 0;
            std::vector<int> nums;
            std::stringstream ss(v);
            std::string part;
            while (std::getline(ss, part, '.'))
            {
                try
                {
                    nums.push_back(std::stoi(part));
                }
                catch (...)
                {
                    nums.push_back(0);
                }
            }
            if (nums.size() > 0) major = nums[0];
            if (nums.size() > 1) minor = nums[1];
            if (nums.size() > 2) patch = nums[2];
            return {major, minor, patch, v};
        }

        std::vector<ForgePromotionEntry> ParsePromotionsJson(const json &j)
        {
            std::map<std::string, ForgePromotionEntry> mapByMc;
            if (!j.is_object() || !j.contains("promos") || !j["promos"].is_object())
                return {};

            for (auto &[key, val] : j["promos"].items())
            {
                if (!val.is_string())
                    continue;
                std::string forgeVer = val.get<std::string>();

                const std::string recSuffix = "-recommended";
                const std::string latSuffix = "-latest";

                if (key.size() > recSuffix.size() &&
                    key.compare(key.size() - recSuffix.size(), recSuffix.size(), recSuffix) == 0)
                {
                    std::string mc = key.substr(0, key.size() - recSuffix.size());
                    auto &entry = mapByMc[mc];
                    entry.mcVersion = mc;
                    entry.recommendedForge = forgeVer;
                }
                else if (key.size() > latSuffix.size() &&
                         key.compare(key.size() - latSuffix.size(), latSuffix.size(), latSuffix) == 0)
                {
                    std::string mc = key.substr(0, key.size() - latSuffix.size());
                    auto &entry = mapByMc[mc];
                    entry.mcVersion = mc;
                    entry.latestForge = forgeVer;
                }
            }

            std::vector<ForgePromotionEntry> out;
            out.reserve(mapByMc.size());
            for (auto &[mc, entry] : mapByMc)
            {
                // Skip pre-release suffixes like 1.7.10_pre4 for cleaner list
                if (mc.find('_') != std::string::npos)
                    continue;
                out.push_back(std::move(entry));
            }

            std::sort(out.begin(), out.end(), [](const ForgePromotionEntry &a, const ForgePromotionEntry &b) {
                return ParseMcVersionTuple(a.mcVersion) > ParseMcVersionTuple(b.mcVersion);
            });

            return out;
        }

        bool EnsureLauncherProfilesExists(const std::string &gameDir)
        {
            std::error_code ec;
            fs::create_directories(gameDir, ec);
            fs::path profilesPath = fs::path(gameDir) / "launcher_profiles.json";
            if (fs::exists(profilesPath, ec) && fs::file_size(profilesPath, ec) > 2)
                return true;

            json minimal = {
                {"profiles", json::object()},
                {"selectedProfile", ""},
                {"clientToken", "00000000-0000-0000-0000-000000000000"}
            };
            std::ofstream out(profilesPath, std::ios::trunc);
            if (!out.is_open())
                return false;
            out << minimal.dump(2);
            return true;
        }

        std::string QuoteWinArgSimple(const std::string &arg)
        {
            if (arg.find_first_of(" \t\"") == std::string::npos && !arg.empty())
                return arg;
            return "\"" + arg + "\"";
        }

        bool RunProcessCaptureOutput(
            const std::string &cmdLine,
            const std::string &workingDir,
            const LogCallback &logCb,
            const StatusCallback &statusCb,
            DWORD &outExitCode)
        {
            SECURITY_ATTRIBUTES sa{};
            sa.nLength = sizeof(sa);
            sa.bInheritHandle = TRUE;

            HANDLE readPipe = nullptr;
            HANDLE writePipe = nullptr;
            if (!CreatePipe(&readPipe, &writePipe, &sa, 0))
                return false;
            SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);

            STARTUPINFOA si{};
            si.cb = sizeof(si);
            si.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;
            si.hStdOutput = writePipe;
            si.hStdError = writePipe;
            si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);

            PROCESS_INFORMATION pi{};
            std::string mutableCmd = cmdLine;
            BOOL ok = CreateProcessA(
                nullptr, mutableCmd.data(), nullptr, nullptr, TRUE,
                CREATE_NO_WINDOW, nullptr,
                workingDir.empty() ? nullptr : workingDir.c_str(),
                &si, &pi);

            CloseHandle(writePipe);

            if (!ok)
            {
                CloseHandle(readPipe);
                return false;
            }

            char buf[2048];
            std::string pending;
            DWORD bytesRead = 0;
            while (ReadFile(readPipe, buf, sizeof(buf) - 1, &bytesRead, nullptr) && bytesRead > 0)
            {
                pending.append(buf, bytesRead);
                size_t pos = 0;
                while ((pos = pending.find('\n')) != std::string::npos)
                {
                    std::string line = pending.substr(0, pos);
                    if (!line.empty() && line.back() == '\r')
                        line.pop_back();
                    if (!line.empty())
                    {
                        if (logCb)
                            logCb("[Forge] " + line);
                        if (statusCb && (line.find("Building") != std::string::npos ||
                                         line.find("Downloading") != std::string::npos ||
                                         line.find("Extracting") != std::string::npos ||
                                         line.find("Patching") != std::string::npos ||
                                         line.find("Remapping") != std::string::npos))
                        {
                            statusCb(line.size() > 72 ? line.substr(0, 69) + "..." : line);
                        }
                    }
                    pending.erase(0, pos + 1);
                }
            }
            if (!pending.empty() && logCb)
                logCb("[Forge] " + pending);

            WaitForSingleObject(pi.hProcess, INFINITE);
            GetExitCodeProcess(pi.hProcess, &outExitCode);

            CloseHandle(readPipe);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
    } // namespace

    std::vector<ForgePromotionEntry> LoadCachedForgePromotions(const std::string &gameDir)
    {
        fs::path cacheFile = fs::path(gameDir) / "launcher_cache" / "forge_promotions.json";
        std::error_code ec;
        if (!fs::exists(cacheFile, ec))
            return {};

        std::ifstream in(cacheFile);
        if (!in.is_open())
            return {};

        json j = json::parse(in, nullptr, false);
        return ParsePromotionsJson(j);
    }

    std::vector<ForgePromotionEntry> FetchForgePromotions(const std::string &gameDir)
    {
        std::string body;
        if (MinecraftLauncher::DownloadString(
                "https://files.minecraftforge.net/net/minecraftforge/forge/promotions_slim.json",
                body))
        {
            json j = json::parse(body, nullptr, false);
            if (j.is_object() && j.contains("promos"))
            {
                std::error_code ec;
                fs::path cacheDir = fs::path(gameDir) / "launcher_cache";
                fs::create_directories(cacheDir, ec);
                std::ofstream out(cacheDir / "forge_promotions.json", std::ios::trunc);
                if (out.is_open())
                    out << body;
                return ParsePromotionsJson(j);
            }
        }
        return LoadCachedForgePromotions(gameDir);
    }

    ForgeJarInspection InspectForgeInstallerJar(const std::string &jarPath)
    {
        ForgeJarInspection info;
        ZipUtils::ZipArchive archive;
        if (!archive.Open(jarPath))
            return info;

        std::string profileText;
        if (!archive.ReadEntryString("install_profile.json", profileText))
            return info;

        json profile = json::parse(profileText, nullptr, false);
        if (!profile.is_object())
            return info;

        std::string versionText;
        if (archive.ReadEntryString("version.json", versionText))
        {
            json ver = json::parse(versionText, nullptr, false);
            if (ver.is_object())
            {
                info.isLegacy = false;
                info.targetVersionId = ver.value("id", profile.value("version", ""));
                info.mcVersion = ver.value("inheritsFrom", profile.value("minecraft", ""));
                info.forgeVersion = profile.value("version", info.targetVersionId);
                info.valid = !info.targetVersionId.empty() && !info.mcVersion.empty();
                return info;
            }
        }

        if (profile.contains("install") && profile["install"].is_object() &&
            profile.contains("versionInfo") && profile["versionInfo"].is_object())
        {
            const auto &inst = profile["install"];
            const auto &vinfo = profile["versionInfo"];
            info.isLegacy = true;
            info.mcVersion = inst.value("minecraft", vinfo.value("inheritsFrom", ""));
            info.targetVersionId = inst.value("target", vinfo.value("id", ""));
            info.forgeVersion = inst.value("version", "");
            info.valid = !info.targetVersionId.empty() && !info.mcVersion.empty();
        }
        return info;
    }

    bool InstallForgeFromInstallerJar(
        const std::string &installerJarPath,
        const std::string &gameDir,
        const std::string &customJavaPath,
        std::atomic<float> &progressOut,
        const StatusCallback &statusCb,
        const LogCallback &logCb,
        std::string &outInstalledVersionId)
    {
        auto setStatus = [&](const std::string &s) {
            if (statusCb)
                statusCb(s);
        };
        auto appendLog = [&](const std::string &s) {
            if (logCb)
                logCb(s);
        };

        if (!fs::exists(installerJarPath))
        {
            setStatus("Forge installer .jar not found!");
            return false;
        }

        ZipUtils::ZipArchive archive;
        if (!archive.Open(installerJarPath))
        {
            setStatus("Failed to open Forge .jar archive.");
            return false;
        }

        std::string profileText;
        if (!archive.ReadEntryString("install_profile.json", profileText))
        {
            setStatus("Selected .jar is not a Forge Installer (missing install_profile.json).");
            return false;
        }

        json profile = json::parse(profileText, nullptr, false);
        if (!profile.is_object())
        {
            setStatus("Invalid install_profile.json inside Forge .jar.");
            return false;
        }

        std::string versionText;
        bool hasModernVersionJson = archive.ReadEntryString("version.json", versionText);
        archive.Close();

        progressOut = 0.05f;

        if (!hasModernVersionJson && profile.contains("install") && profile.contains("versionInfo"))
        {
            // Legacy Forge Installer (MC 1.7.10 - 1.12.2)
            const auto &inst = profile["install"];
            json verInfo = profile["versionInfo"];
            std::string mcVer = inst.value("minecraft", verInfo.value("inheritsFrom", ""));
            std::string targetId = inst.value("target", verInfo.value("id", ""));
            std::string universalEntry = inst.value("filePath", "");
            std::string universalCoord = inst.value("path", "");

            if (mcVer.empty() || targetId.empty())
            {
                setStatus("Invalid legacy Forge install profile.");
                return false;
            }

            appendLog("[Forge] Installing legacy Forge " + targetId + " (base MC " + mcVer + ")");
            setStatus("Preparing vanilla Minecraft " + mcVer + "...");

            json baseData;
            if (!MinecraftLauncher::EnsureVersionDownloaded(gameDir, mcVer, baseData, progressOut, "", statusCb, logCb))
            {
                setStatus("Failed to download base Minecraft " + mcVer + " for Forge.");
                return false;
            }

            progressOut = 0.65f;
            if (!universalEntry.empty() && !universalCoord.empty())
            {
                std::string relPath = MinecraftLauncher::MavenCoordToRelativePath(universalCoord);
                std::replace(relPath.begin(), relPath.end(), '/', '\\');
                fs::path dstLib = fs::path(gameDir) / "libraries" / relPath;
                setStatus("Extracting Forge universal library...");
                if (!ZipUtils::ExtractSingleEntryToFile(installerJarPath, universalEntry, dstLib.string()))
                {
                    appendLog("[Forge] Warning: Could not extract embedded universal jar: " + universalEntry);
                }
            }

            if (!verInfo.contains("inheritsFrom"))
                verInfo["inheritsFrom"] = mcVer;
            verInfo["id"] = targetId;

            std::error_code ec;
            fs::path targetDir = fs::path(gameDir) / "versions" / targetId;
            fs::create_directories(targetDir, ec);
            std::ofstream vOut(targetDir / (targetId + ".json"), std::ios::trunc);
            if (!vOut.is_open())
            {
                setStatus("Failed to write Forge version JSON.");
                return false;
            }
            vOut << verInfo.dump(2);
            vOut.close();

            progressOut = 0.75f;
            setStatus("Downloading Forge libraries...");
            json resolved;
            if (!MinecraftLauncher::EnsureVersionDownloaded(gameDir, targetId, resolved, progressOut, "", statusCb, logCb))
            {
                setStatus("Failed to download some Forge dependencies.");
                return false;
            }

            outInstalledVersionId = targetId;
            progressOut = 1.0f;
            setStatus("Forge " + targetId + " installed!");
            appendLog("[Forge] Successfully installed " + targetId);
            return true;
        }

        if (!hasModernVersionJson)
        {
            setStatus("Unsupported Forge installer format.");
            return false;
        }

        // Modern Forge Installer (MC 1.13 - 1.21+)
        json verJson = json::parse(versionText, nullptr, false);
        if (!verJson.is_object())
        {
            setStatus("Invalid version.json inside Forge installer.");
            return false;
        }

        std::string targetId = verJson.value("id", "");
        std::string mcVer = verJson.value("inheritsFrom", profile.value("minecraft", ""));
        int requiredJava = 17;
        if (verJson.contains("javaVersion") && verJson["javaVersion"].is_object())
        {
            requiredJava = verJson["javaVersion"].value("majorVersion", 17);
        }

        if (targetId.empty() || mcVer.empty())
        {
            setStatus("Could not determine target version from Forge installer.");
            return false;
        }

        appendLog("[Forge] Installing modern Forge " + targetId + " (base MC " + mcVer + ")");
        setStatus("Preparing vanilla Minecraft " + mcVer + "...");

        json baseData;
        if (!MinecraftLauncher::EnsureVersionDownloaded(gameDir, mcVer, baseData, progressOut, "", statusCb, logCb))
        {
            setStatus("Failed to download vanilla Minecraft " + mcVer + ".");
            return false;
        }

        EnsureLauncherProfilesExists(gameDir);

        auto runtimes = PlatformUtils::ScanJavaRuntimes();
        std::string javaExe = PlatformUtils::PickBestJavaExecutable(runtimes, requiredJava, customJavaPath);
        appendLog("[Forge] Running installer with Java: " + javaExe);

        setStatus("Running Forge installer (patching & downloading libraries)...");
        progressOut = 0.65f;

        std::string cmd = QuoteWinArgSimple(javaExe) +
                          " -jar " + QuoteWinArgSimple(installerJarPath) +
                          " --installClient " + QuoteWinArgSimple(gameDir);

        DWORD exitCode = 1;
        bool launched = RunProcessCaptureOutput(cmd, gameDir, logCb, statusCb, exitCode);
        if (!launched || exitCode != 0)
        {
            appendLog("[Forge] Installer failed with exit code " + std::to_string(exitCode));
            setStatus("Forge installer failed (check Launcher Log for details).");
            return false;
        }

        fs::path installedJson = fs::path(gameDir) / "versions" / targetId / (targetId + ".json");
        std::error_code ec;
        if (!fs::exists(installedJson, ec))
        {
            setStatus("Forge installer completed but version JSON was not found.");
            return false;
        }

        setStatus("Verifying installed Forge files...");
        json resolved;
        MinecraftLauncher::EnsureVersionDownloaded(gameDir, targetId, resolved, progressOut, "", statusCb, logCb);

        outInstalledVersionId = targetId;
        progressOut = 1.0f;
        setStatus("Forge " + targetId + " installed!");
        appendLog("[Forge] Successfully installed " + targetId);
        return true;
    }

    bool DownloadAndInstallForge(
        const std::string &mcVersion,
        const std::string &forgeVersion,
        const std::string &gameDir,
        const std::string &customJavaPath,
        std::atomic<float> &progressOut,
        const StatusCallback &statusCb,
        const LogCallback &logCb,
        std::string &outInstalledVersionId)
    {
        if (mcVersion.empty() || forgeVersion.empty())
            return false;

        std::string fullVer = mcVersion + "-" + forgeVersion;
        std::string urlPrimary = "https://maven.minecraftforge.net/net/minecraftforge/forge/" +
                                 fullVer + "/forge-" + fullVer + "-installer.jar";
        std::string fullLegacy = mcVersion + "-" + forgeVersion + "-" + mcVersion;
        std::string urlFallback = "https://maven.minecraftforge.net/net/minecraftforge/forge/" +
                                  fullLegacy + "/forge-" + fullLegacy + "-installer.jar";

        fs::path installerCache = fs::path(gameDir) / "launcher_cache" / ("forge-" + fullVer + "-installer.jar");

        if (statusCb)
            statusCb("Downloading Forge " + fullVer + " installer...");
        if (logCb)
            logCb("[Forge] Downloading " + urlPrimary);
        progressOut = 0.05f;

        if (!fs::exists(installerCache) || fs::file_size(installerCache) < 10240)
        {
            if (!MinecraftLauncher::DownloadFile(urlPrimary, installerCache.string()))
            {
                if (logCb)
                    logCb("[Forge] Trying legacy URL " + urlFallback);
                if (!MinecraftLauncher::DownloadFile(urlFallback, installerCache.string()))
                {
                    if (statusCb)
                        statusCb("Failed to download Forge installer .jar!");
                    return false;
                }
            }
        }

        return InstallForgeFromInstallerJar(
            installerCache.string(),
            gameDir,
            customJavaPath,
            progressOut,
            statusCb,
            logCb,
            outInstalledVersionId);
    }
} // namespace ForgeInstaller
