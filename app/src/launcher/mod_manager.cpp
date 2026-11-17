#include "launcher/mod_manager.hpp"
#include "launcher/zip_utils.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>
#include <unordered_map>

namespace fs = std::filesystem;
using json = nlohmann::json;

namespace ModManager
{
    namespace
    {
        std::string ToLowerAscii(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });
            return s;
        }

        std::string Trim(std::string s)
        {
            auto notSpace = [](unsigned char c) { return !std::isspace(c); };
            s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
            s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
            return s;
        }

        std::string StripTomlQuotes(std::string v)
        {
            v = Trim(std::move(v));
            if (v.size() >= 6 &&
                ((v.rfind("\"\"\"", 0) == 0 && v.compare(v.size() - 3, 3, "\"\"\"") == 0) ||
                 (v.rfind("'''", 0) == 0 && v.compare(v.size() - 3, 3, "'''") == 0)))
            {
                return Trim(v.substr(3, v.size() - 6));
            }
            if (v.size() >= 2 &&
                ((v.front() == '"' && v.back() == '"') ||
                 (v.front() == '\'' && v.back() == '\'')))
            {
                return v.substr(1, v.size() - 2);
            }
            return v;
        }

        std::string StripTomlInlineComment(const std::string &line)
        {
            bool inSingle = false;
            bool inDouble = false;
            bool escaped = false;
            for (size_t i = 0; i < line.size(); ++i)
            {
                char c = line[i];
                if (escaped)
                {
                    escaped = false;
                    continue;
                }
                if (c == '\\' && inDouble)
                {
                    escaped = true;
                    continue;
                }
                if (c == '\'' && !inDouble)
                    inSingle = !inSingle;
                else if (c == '"' && !inSingle)
                    inDouble = !inDouble;
                else if (c == '#' && !inSingle && !inDouble)
                    return line.substr(0, i);
            }
            return line;
        }

        struct ManifestInfo
        {
            std::string title;
            std::string version;
            std::string author;
        };

        ManifestInfo ParseManifest(const std::string &manifestText)
        {
            ManifestInfo info;
            std::istringstream iss(manifestText);
            std::string line;
            while (std::getline(iss, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();
                size_t colon = line.find(':');
                if (colon == std::string::npos)
                    continue;
                std::string key = ToLowerAscii(Trim(line.substr(0, colon)));
                std::string val = Trim(line.substr(colon + 1));
                if (key == "implementation-title" && info.title.empty())
                    info.title = val;
                else if (key == "implementation-version" && info.version.empty())
                    info.version = val;
                else if (key == "implementation-vendor" && info.author.empty())
                    info.author = val;
            }
            return info;
        }

        void ParseModsToml(const std::string &tomlText, ModInfo &outMod, std::string &outLogoFile)
        {
            std::istringstream iss(tomlText);
            std::string line;
            std::string section;
            bool capturedFirstMod = false;
            std::string currentDepModId;
            std::string currentDepVersionRange;

            bool inMultiline = false;
            std::string multiKey;
            std::string multiDelim;
            std::string multiBuffer;

            auto flushKeyVal = [&](const std::string &sec, const std::string &key, const std::string &val) {
                std::string k = ToLowerAscii(Trim(key));
                std::string v = StripTomlQuotes(val);

                if (sec.empty())
                {
                    if (k == "modloader" && outMod.loaderInfo.empty())
                        outMod.loaderInfo = v;
                }
                else if (sec == "mods" && !capturedFirstMod)
                {
                    if (k == "modid")
                        outMod.modId = v;
                    else if (k == "version")
                        outMod.version = v;
                    else if (k == "displayname")
                        outMod.displayName = v;
                    else if (k == "authors")
                        outMod.authors = v;
                    else if (k == "description")
                        outMod.description = v;
                    else if (k == "logofile")
                        outLogoFile = v;
                }
                else if (sec.rfind("dependencies", 0) == 0)
                {
                    if (k == "modid")
                        currentDepModId = ToLowerAscii(v);
                    else if (k == "versionrange")
                        currentDepVersionRange = v;

                    if (currentDepModId == "minecraft" && !currentDepVersionRange.empty() && outMod.mcVersion.empty())
                    {
                        outMod.mcVersion = currentDepVersionRange;
                    }
                }
            };

            while (std::getline(iss, line))
            {
                if (!line.empty() && line.back() == '\r')
                    line.pop_back();

                if (inMultiline)
                {
                    size_t endPos = line.find(multiDelim);
                    if (endPos != std::string::npos)
                    {
                        multiBuffer += line.substr(0, endPos);
                        flushKeyVal(section, multiKey, multiBuffer);
                        inMultiline = false;
                        multiBuffer.clear();
                    }
                    else
                    {
                        multiBuffer += line + "\n";
                    }
                    continue;
                }

                std::string clean = Trim(StripTomlInlineComment(line));
                if (clean.empty())
                    continue;

                if (clean.front() == '[')
                {
                    if (section == "mods" && !outMod.modId.empty())
                    {
                        capturedFirstMod = true;
                    }
                    currentDepModId.clear();
                    currentDepVersionRange.clear();

                    std::string header = clean;
                    while (!header.empty() && (header.front() == '[' || header.front() == ' '))
                        header.erase(header.begin());
                    while (!header.empty() && (header.back() == ']' || header.back() == ' '))
                        header.pop_back();
                    section = ToLowerAscii(Trim(header));
                    continue;
                }

                size_t eq = clean.find('=');
                if (eq == std::string::npos)
                    continue;

                std::string key = Trim(clean.substr(0, eq));
                std::string rawVal = Trim(clean.substr(eq + 1));

                auto checkMulti = [&](const std::string &delim) -> bool {
                    if (rawVal.rfind(delim, 0) == 0)
                    {
                        std::string rest = rawVal.substr(delim.size());
                        size_t closePos = rest.find(delim);
                        if (closePos != std::string::npos)
                        {
                            flushKeyVal(section, key, rest.substr(0, closePos));
                        }
                        else
                        {
                            inMultiline = true;
                            multiKey = key;
                            multiDelim = delim;
                            multiBuffer = rest + "\n";
                        }
                        return true;
                    }
                    return false;
                };

                if (checkMulti("\"\"\"") || checkMulti("'''"))
                    continue;

                flushKeyVal(section, key, rawVal);
            }
        }

        void ParseMcModInfo(const std::string &jsonText, ModInfo &outMod, std::string &outLogoFile)
        {
            json j = json::parse(jsonText, nullptr, false);
            if (j.is_discarded())
                return;

            json entry;
            if (j.is_array() && !j.empty())
                entry = j[0];
            else if (j.is_object() && j.contains("modList") && j["modList"].is_array() && !j["modList"].empty())
                entry = j["modList"][0];
            else if (j.is_object())
                entry = j;

            if (!entry.is_object())
                return;

            outMod.modId = entry.value("modid", outMod.modId);
            outMod.displayName = entry.value("name", outMod.displayName);
            outMod.version = entry.value("version", outMod.version);
            outMod.description = entry.value("description", outMod.description);
            outMod.mcVersion = entry.value("mcversion", outMod.mcVersion);
            outLogoFile = entry.value("logoFile", outLogoFile);

            if (entry.contains("authorList") && entry["authorList"].is_array())
            {
                std::string authors;
                for (const auto &a : entry["authorList"])
                {
                    if (a.is_string())
                    {
                        if (!authors.empty())
                            authors += ", ";
                        authors += a.get<std::string>();
                    }
                }
                if (!authors.empty())
                    outMod.authors = authors;
            }
        }

        void ParseFabricModJson(const std::string &jsonText, ModInfo &outMod, std::string &outLogoFile)
        {
            json j = json::parse(jsonText, nullptr, false);
            if (!j.is_object())
                return;

            outMod.modId = j.value("id", outMod.modId);
            outMod.displayName = j.value("name", outMod.displayName);
            outMod.version = j.value("version", outMod.version);
            outMod.description = j.value("description", outMod.description);
            if (j.contains("icon") && j["icon"].is_string())
                outLogoFile = j["icon"].get<std::string>();

            if (j.contains("depends") && j["depends"].is_object() && j["depends"].contains("minecraft"))
            {
                const auto &mc = j["depends"]["minecraft"];
                if (mc.is_string())
                    outMod.mcVersion = mc.get<std::string>();
            }
        }

        std::string BaseNameWithoutModExtension(const std::string &fileName)
        {
            std::string lower = ToLowerAscii(fileName);
            if (lower.size() > 13 && lower.compare(lower.size() - 13, 13, ".jar.disabled") == 0)
                return fileName.substr(0, fileName.size() - 13);
            if (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".jar") == 0)
                return fileName.substr(0, fileName.size() - 4);
            return fileName;
        }
    } // namespace

    JarKind DetectJarKind(const std::string &jarPath, std::string *outLabel)
    {
        ZipUtils::ZipArchive archive;
        if (!archive.Open(jarPath))
        {
            if (outLabel)
                *outLabel = "Invalid or unreadable .jar";
            return JarKind::Unknown;
        }

        if (archive.HasEntry("install_profile.json"))
        {
            if (outLabel)
                *outLabel = "Forge Installer (.jar)";
            return JarKind::ForgeInstaller;
        }
        if (archive.HasEntry("META-INF/neoforge.mods.toml"))
        {
            if (outLabel)
                *outLabel = "NeoForge Mod (.jar)";
            return JarKind::NeoForgeMod;
        }
        if (archive.HasEntry("META-INF/mods.toml") || archive.HasEntry("mcmod.info"))
        {
            if (outLabel)
                *outLabel = "Forge Mod (.jar)";
            return JarKind::ForgeMod;
        }
        if (archive.HasEntry("fabric.mod.json") || archive.HasEntry("quilt.mod.json"))
        {
            if (outLabel)
                *outLabel = "Fabric/Quilt Mod (.jar)";
            return JarKind::FabricMod;
        }

        if (outLabel)
            *outLabel = "Generic Java/Mod (.jar)";
        return JarKind::Unknown;
    }

    ModInfo InspectModJar(const std::string &filePath)
    {
        ModInfo mod;
        fs::path p(filePath);
        mod.filePath = filePath;
        mod.fileName = p.filename().string();

        std::string lowerName = ToLowerAscii(mod.fileName);
        mod.enabled = !(lowerName.size() >= 9 && lowerName.compare(lowerName.size() - 9, 9, ".disabled") == 0);

        std::error_code ec;
        mod.fileSize = fs::file_size(p, ec);
        auto wt = fs::last_write_time(p, ec);
        if (!ec)
            mod.lastWriteTicks = wt.time_since_epoch().count();

        ZipUtils::ZipArchive archive;
        std::string logoFile;
        ManifestInfo manifest;

        if (archive.Open(filePath))
        {
            std::string manifestText;
            if (archive.ReadEntryString("META-INF/MANIFEST.MF", manifestText))
            {
                manifest = ParseManifest(manifestText);
            }

            std::string text;
            if (archive.HasEntry("install_profile.json"))
            {
                mod.kind = JarKind::ForgeInstaller;
                mod.loaderInfo = "Forge Installer";
            }
            else if (archive.ReadEntryString("META-INF/neoforge.mods.toml", text))
            {
                mod.kind = JarKind::NeoForgeMod;
                ParseModsToml(text, mod, logoFile);
            }
            else if (archive.ReadEntryString("META-INF/mods.toml", text))
            {
                mod.kind = JarKind::ForgeMod;
                ParseModsToml(text, mod, logoFile);
            }
            else if (archive.ReadEntryString("mcmod.info", text))
            {
                mod.kind = JarKind::ForgeMod;
                ParseMcModInfo(text, mod, logoFile);
            }
            else if (archive.ReadEntryString("fabric.mod.json", text))
            {
                mod.kind = JarKind::FabricMod;
                ParseFabricModJson(text, mod, logoFile);
            }

            if (!logoFile.empty())
            {
                std::vector<uint8_t> bytes;
                if (archive.ReadEntryBytes(logoFile, bytes) && bytes.size() <= 1024 * 1024)
                {
                    mod.iconBytes = std::move(bytes);
                }
            }
        }

        if (mod.version.empty() || mod.version.find("${") != std::string::npos)
        {
            if (!manifest.version.empty())
                mod.version = manifest.version;
            else if (mod.version.find("${") != std::string::npos)
                mod.version.clear();
        }
        if (mod.displayName.empty())
        {
            mod.displayName = !manifest.title.empty() ? manifest.title : BaseNameWithoutModExtension(mod.fileName);
        }
        if (mod.authors.empty() && !manifest.author.empty())
        {
            mod.authors = manifest.author;
        }
        if (mod.description.size() > 220)
        {
            mod.description = mod.description.substr(0, 217) + "...";
        }
        // Flatten multiline description whitespace for crisp card layout
        for (char &c : mod.description)
        {
            if (c == '\n' || c == '\r' || c == '\t')
                c = ' ';
        }

        return mod;
    }

    std::vector<ModInfo> ScanModsDirectory(
        const std::string &modsDir,
        const std::vector<ModInfo> &previousCache)
    {
        std::vector<ModInfo> result;
        std::error_code ec;
        if (modsDir.empty() || !fs::exists(modsDir, ec) || !fs::is_directory(modsDir, ec))
            return result;

        std::unordered_map<std::string, const ModInfo *> cacheMap;
        cacheMap.reserve(previousCache.size());
        for (const auto &m : previousCache)
        {
            cacheMap[m.filePath] = &m;
        }

        for (const auto &entry : fs::directory_iterator(modsDir, ec))
        {
            if (!entry.is_regular_file(ec))
                continue;

            std::string fileName = entry.path().filename().string();
            std::string lower = ToLowerAscii(fileName);
            bool isJar = (lower.size() > 4 && lower.compare(lower.size() - 4, 4, ".jar") == 0);
            bool isDisabledJar = (lower.size() > 13 && lower.compare(lower.size() - 13, 13, ".jar.disabled") == 0);
            if (!isJar && !isDisabledJar)
                continue;

            std::string fullPath = entry.path().string();
            uint64_t size = fs::file_size(entry.path(), ec);
            auto wt = fs::last_write_time(entry.path(), ec);
            int64_t ticks = ec ? 0 : wt.time_since_epoch().count();

            auto it = cacheMap.find(fullPath);
            if (it != cacheMap.end() && it->second->fileSize == size && it->second->lastWriteTicks == ticks)
            {
                result.push_back(*it->second);
            }
            else
            {
                result.push_back(InspectModJar(fullPath));
            }
        }

        std::sort(result.begin(), result.end(), [](const ModInfo &a, const ModInfo &b) {
            if (a.enabled != b.enabled)
                return a.enabled > b.enabled;
            return ToLowerAscii(a.displayName) < ToLowerAscii(b.displayName);
        });

        return result;
    }

    bool ImportModJar(
        const std::string &sourcePath,
        const std::string &modsDir,
        std::string &outDestPath,
        std::string &outError)
    {
        std::error_code ec;
        if (!fs::exists(sourcePath, ec) || !fs::is_regular_file(sourcePath, ec))
        {
            outError = "Source .jar file does not exist.";
            return false;
        }

        fs::create_directories(modsDir, ec);
        if (ec)
        {
            outError = "Failed to create mods directory: " + ec.message();
            return false;
        }

        fs::path src(sourcePath);
        std::string fileName = src.filename().string();
        std::string lower = ToLowerAscii(fileName);
        if (lower.size() < 4 || lower.compare(lower.size() - 4, 4, ".jar") != 0)
        {
            outError = "Selected file is not a .jar file.";
            return false;
        }

        fs::path dst = fs::path(modsDir) / fileName;
        outDestPath = dst.string();

        if (fs::equivalent(src, dst, ec))
            return true;

        fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
        if (ec)
        {
            outError = "Failed to copy mod .jar: " + ec.message();
            return false;
        }
        return true;
    }

    bool SetModEnabled(ModInfo &mod, bool enable, std::string &outError)
    {
        if (mod.enabled == enable)
            return true;

        fs::path oldPath(mod.filePath);
        std::string newPathStr = mod.filePath;
        std::string lower = ToLowerAscii(newPathStr);

        if (enable)
        {
            if (lower.size() > 9 && lower.compare(lower.size() - 9, 9, ".disabled") == 0)
            {
                newPathStr = newPathStr.substr(0, newPathStr.size() - 9);
            }
        }
        else
        {
            if (lower.size() < 9 || lower.compare(lower.size() - 9, 9, ".disabled") != 0)
            {
                newPathStr += ".disabled";
            }
        }

        std::error_code ec;
        fs::rename(oldPath, fs::path(newPathStr), ec);
        if (ec)
        {
            outError = "Failed to toggle mod: " + ec.message();
            return false;
        }

        mod.filePath = newPathStr;
        mod.fileName = fs::path(newPathStr).filename().string();
        mod.enabled = enable;
        return true;
    }

    bool DeleteModFile(const ModInfo &mod, std::string &outError)
    {
        std::error_code ec;
        if (!fs::remove(fs::path(mod.filePath), ec))
        {
            outError = ec ? ("Failed to delete mod: " + ec.message()) : "Mod file not found.";
            return false;
        }
        return true;
    }

    std::string GetLoaderDisplayName(JarKind kind)
    {
        switch (kind)
        {
        case JarKind::ForgeInstaller:
            return "Forge Installer";
        case JarKind::ForgeMod:
            return "Forge";
        case JarKind::NeoForgeMod:
            return "NeoForge";
        case JarKind::FabricMod:
            return "Fabric";
        default:
            return "Mod JAR";
        }
    }
} // namespace ModManager
