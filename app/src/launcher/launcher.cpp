#include "launcher/launcher.hpp"
#include "launcher/zip_utils.hpp"
#include "platform_utils.hpp"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <wincrypt.h>
#include <urlmon.h>

#include <httplib.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <thread>
#include <unordered_map>

namespace fs = std::filesystem;

namespace
{
    std::string ToLowerAscii(std::string s)
    {
        std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) {
            return static_cast<char>(std::tolower(c));
        });
        return s;
    }

    std::string NormalizeSlashes(std::string path)
    {
        std::replace(path.begin(), path.end(), '\\', '/');
        return path;
    }

    std::string ToBackslashes(std::string path)
    {
        std::replace(path.begin(), path.end(), '/', '\\');
        return path;
    }

    std::string QuoteWinArg(const std::string &arg)
    {
        if (arg.empty())
            return "\"\"";
        bool needsQuotes = (arg.find_first_of(" \t\n\v\";&|<>^") != std::string::npos);
        if (!needsQuotes)
            return arg;

        std::string out = "\"";
        size_t backslashes = 0;
        for (char c : arg)
        {
            if (c == '\\')
            {
                backslashes++;
            }
            else if (c == '"')
            {
                out.append(backslashes * 2 + 1, '\\');
                out.push_back('"');
                backslashes = 0;
            }
            else
            {
                out.append(backslashes, '\\');
                backslashes = 0;
                out.push_back(c);
            }
        }
        out.append(backslashes * 2, '\\');
        out.push_back('"');
        return out;
    }

    bool ParseUrl(const std::string &url, bool &isHttps, std::string &host, int &port, std::string &path)
    {
        std::string s = url;
        isHttps = true;
        port = 443;

        if (s.rfind("https://", 0) == 0)
        {
            s = s.substr(8);
            isHttps = true;
            port = 443;
        }
        else if (s.rfind("http://", 0) == 0)
        {
            s = s.substr(7);
            isHttps = false;
            port = 80;
        }
        else
        {
            return false;
        }

        size_t slash = s.find('/');
        std::string authority = (slash == std::string::npos) ? s : s.substr(0, slash);
        path = (slash == std::string::npos) ? "/" : s.substr(slash);

        size_t colon = authority.rfind(':');
        if (colon != std::string::npos)
        {
            host = authority.substr(0, colon);
            try
            {
                port = std::stoi(authority.substr(colon + 1));
            }
            catch (...)
            {
            }
        }
        else
        {
            host = authority;
        }
        return !host.empty();
    }

    // Thread-local keep-alive HTTP/HTTPS connection pool for fast multi-threaded asset & library downloads
    httplib::SSLClient *GetPooledSSLClient(const std::string &host, int port)
    {
        thread_local std::unordered_map<std::string, std::unique_ptr<httplib::SSLClient>> pool;
        std::string key = host + ":" + std::to_string(port);
        auto it = pool.find(key);
        if (it == pool.end())
        {
            auto cli = std::make_unique<httplib::SSLClient>(host, port);
            cli->enable_server_certificate_verification(false);
            cli->set_follow_location(true);
            cli->set_keep_alive(true);
            cli->set_connection_timeout(10, 0);
            cli->set_read_timeout(30, 0);
            auto *ptr = cli.get();
            pool.emplace(std::move(key), std::move(cli));
            return ptr;
        }
        return it->second.get();
    }

    bool HttpGetBodyInternal(const std::string &url, std::string &outBody, int redirectDepth = 0)
    {
        if (redirectDepth > 5)
            return false;

        bool isHttps = true;
        std::string host, path;
        int port = 443;
        if (!ParseUrl(url, isHttps, host, port, path))
            return false;

        httplib::Headers headers = {
            {"User-Agent", "NolanMinecraftLauncher/2.0 (Windows NT 10.0; Win64; x64)"}};

        if (isHttps)
        {
            for (int attempt = 0; attempt < 2; ++attempt)
            {
                httplib::SSLClient *cli = GetPooledSSLClient(host, port);
                auto res = cli->Get(path, headers);
                if (res)
                {
                    if ((res->status == 301 || res->status == 302 || res->status == 307 || res->status == 308) &&
                        res->has_header("Location"))
                    {
                        return HttpGetBodyInternal(res->get_header_value("Location"), outBody, redirectDepth + 1);
                    }
                    if (res->status == 200)
                    {
                        outBody = std::move(res->body);
                        return true;
                    }
                    return false;
                }
                // Transient keep-alive drop: create fresh client on retry
                httplib::SSLClient fresh(host, port);
                fresh.enable_server_certificate_verification(false);
                fresh.set_follow_location(true);
                fresh.set_connection_timeout(10, 0);
                fresh.set_read_timeout(30, 0);
                auto retryRes = fresh.Get(path, headers);
                if (retryRes && retryRes->status == 200)
                {
                    outBody = std::move(retryRes->body);
                    return true;
                }
                break;
            }
        }
        else
        {
            httplib::Client cli(host, port);
            cli.set_follow_location(true);
            cli.set_connection_timeout(10, 0);
            cli.set_read_timeout(30, 0);
            auto res = cli.Get(path, headers);
            if (res && (res->status == 301 || res->status == 302) && res->has_header("Location"))
            {
                return HttpGetBodyInternal(res->get_header_value("Location"), outBody, redirectDepth + 1);
            }
            if (res && res->status == 200)
            {
                outBody = std::move(res->body);
                return true;
            }
        }
        return false;
    }

    bool IsAllowedForWindowsX64(const std::string &path)
    {
        std::string lower = ToLowerAscii(path);
        if (lower.find("linux") != std::string::npos) return false;
        if (lower.find("macos") != std::string::npos) return false;
        if (lower.find("osx") != std::string::npos) return false;
        if (lower.find("freebsd") != std::string::npos) return false;
        if (lower.find("windows-x86") != std::string::npos) return false;
        if (lower.find("windows-arm64") != std::string::npos) return false;
        return true;
    }

    bool EvaluateRules(const json &rules, bool checkFeatures = true)
    {
        if (!rules.is_array() || rules.empty())
            return true;

        bool allowed = false;
        for (const auto &rule : rules)
        {
            std::string action = rule.value("action", "");
            bool matches = true;

            if (rule.contains("os") && rule["os"].is_object())
            {
                const auto &osObj = rule["os"];
                if (osObj.contains("name") && osObj["name"].is_string())
                {
                    if (osObj["name"].get<std::string>() != "windows")
                        matches = false;
                }
                if (osObj.contains("arch") && osObj["arch"].is_string())
                {
                    std::string arch = ToLowerAscii(osObj["arch"].get<std::string>());
                    if (arch == "x86" || arch == "arm64")
                        matches = false;
                }
            }

            if (checkFeatures && rule.contains("features") && rule["features"].is_object())
            {
                // Launcher does not enable demo mode, quick play, or forced custom resolution flags by default
                matches = false;
            }

            if (matches)
            {
                if (action == "allow")
                    allowed = true;
                else if (action == "disallow")
                    allowed = false;
            }
        }
        return allowed;
    }

    bool ShouldIncludeLibrary(const json &lib)
    {
        if (lib.contains("rules") && !EvaluateRules(lib["rules"], false))
            return false;

        if (lib.contains("downloads") && lib["downloads"].is_object() && lib["downloads"].contains("artifact"))
        {
            std::string libPath = lib["downloads"]["artifact"].value("path", "");
            if (!libPath.empty() && !IsAllowedForWindowsX64(libPath))
                return false;
        }
        else if (lib.contains("name") && lib["name"].is_string())
        {
            std::string coord = lib["name"].get<std::string>();
            if (!IsAllowedForWindowsX64(coord))
                return false;
        }
        return true;
    }

    // Group:Artifact[:Classifier] key to deduplicate libraries across inheritsFrom
    std::string LibraryIdentityKey(const json &lib)
    {
        std::string name = lib.value("name", "");
        if (name.empty())
            return "";
        std::vector<std::string> parts;
        std::stringstream ss(name);
        std::string tok;
        while (std::getline(ss, tok, ':'))
            parts.push_back(tok);
        if (parts.size() >= 2)
        {
            std::string key = parts[0] + ":" + parts[1];
            if (parts.size() >= 4)
                key += ":" + parts[3];
            return key;
        }
        return name;
    }

    std::string FixMavenRepoUrl(std::string repoUrl)
    {
        if (repoUrl.empty())
            return "https://libraries.minecraft.net/";
        if (repoUrl.rfind("http://files.minecraftforge.net/maven", 0) == 0)
            repoUrl = "https://maven.minecraftforge.net/";
        else if (repoUrl.rfind("http://", 0) == 0)
            repoUrl = "https://" + repoUrl.substr(7);

        if (repoUrl.back() != '/')
            repoUrl.push_back('/');
        return repoUrl;
    }

    struct LibArtifactInfo
    {
        std::string relPath;
        std::string url;
        std::string fallbackUrl;
        size_t size = 0;
        std::string sha1;
        bool isNative = false;
        std::vector<std::string> excludePrefixes;
    };

    std::vector<LibArtifactInfo> CollectLibraryArtifacts(const json &lib)
    {
        std::vector<LibArtifactInfo> items;
        if (!ShouldIncludeLibrary(lib))
            return items;

        std::vector<std::string> excludes = {"META-INF/"};
        if (lib.contains("extract") && lib["extract"].is_object() && lib["extract"].contains("exclude"))
        {
            excludes.clear();
            for (const auto &ex : lib["extract"]["exclude"])
            {
                if (ex.is_string())
                    excludes.push_back(ex.get<std::string>());
            }
        }

        if (lib.contains("downloads") && lib["downloads"].is_object())
        {
            const auto &dl = lib["downloads"];
            if (dl.contains("artifact") && dl["artifact"].is_object())
            {
                const auto &art = dl["artifact"];
                std::string relPath = art.value("path", "");
                if (relPath.empty() && lib.contains("name"))
                    relPath = MinecraftLauncher::MavenCoordToRelativePath(lib["name"].get<std::string>());

                if (!relPath.empty() && IsAllowedForWindowsX64(relPath))
                {
                    LibArtifactInfo info;
                    info.relPath = relPath;
                    info.url = art.value("url", "");
                    info.size = art.value("size", static_cast<size_t>(0));
                    info.sha1 = art.value("sha1", "");
                    info.isNative = (lib.contains("natives") || ToLowerAscii(relPath).find("native") != std::string::npos);
                    info.excludePrefixes = excludes;
                    items.push_back(std::move(info));
                }
            }

            if (lib.contains("natives") && lib["natives"].is_object() &&
                dl.contains("classifiers") && dl["classifiers"].is_object())
            {
                std::string nativeKey = lib["natives"].value("windows", "");
                size_t archPos = nativeKey.find("${arch}");
                if (archPos != std::string::npos)
                    nativeKey.replace(archPos, 7, "64");

                const auto &classifiers = dl["classifiers"];
                const json *natObj = nullptr;
                if (!nativeKey.empty() && classifiers.contains(nativeKey))
                    natObj = &classifiers[nativeKey];
                else if (classifiers.contains("natives-windows"))
                    natObj = &classifiers["natives-windows"];
                else if (classifiers.contains("natives-windows-64"))
                    natObj = &classifiers["natives-windows-64"];

                if (natObj && natObj->is_object())
                {
                    LibArtifactInfo info;
                    info.relPath = natObj->value("path", "");
                    info.url = natObj->value("url", "");
                    info.size = natObj->value("size", static_cast<size_t>(0));
                    info.sha1 = natObj->value("sha1", "");
                    info.isNative = true;
                    info.excludePrefixes = excludes;
                    if (!info.relPath.empty())
                        items.push_back(std::move(info));
                }
            }
        }
        else if (lib.contains("name") && lib["name"].is_string())
        {
            std::string relPath = MinecraftLauncher::MavenCoordToRelativePath(lib["name"].get<std::string>());
            if (!relPath.empty())
            {
                std::string baseRepo = FixMavenRepoUrl(lib.value("url", ""));
                LibArtifactInfo info;
                info.relPath = relPath;
                info.url = baseRepo + relPath;
                if (baseRepo.find("minecraftforge") != std::string::npos)
                {
                    info.fallbackUrl = "https://libraries.minecraft.net/" + relPath;
                }
                else
                {
                    info.fallbackUrl = "https://repo1.maven.org/maven2/" + relPath;
                }
                info.isNative = lib.contains("natives") || (ToLowerAscii(relPath).find("native") != std::string::npos);
                info.excludePrefixes = excludes;
                items.push_back(std::move(info));
            }
        }

        return items;
    }

    std::string ReplaceVars(std::string input, const std::unordered_map<std::string, std::string> &vars)
    {
        for (const auto &[k, v] : vars)
        {
            std::string token = "${" + k + "}";
            size_t pos = 0;
            while ((pos = input.find(token, pos)) != std::string::npos)
            {
                input.replace(pos, token.size(), v);
                pos += v.size();
            }
        }
        return input;
    }

    void ExpandArgumentList(
        const json &argArray,
        const std::unordered_map<std::string, std::string> &vars,
        std::vector<std::string> &outArgs)
    {
        if (!argArray.is_array())
            return;

        for (const auto &item : argArray)
        {
            if (item.is_string())
            {
                outArgs.push_back(ReplaceVars(item.get<std::string>(), vars));
            }
            else if (item.is_object())
            {
                if (item.contains("rules") && !EvaluateRules(item["rules"], true))
                    continue;
                if (!item.contains("value"))
                    continue;

                const auto &val = item["value"];
                if (val.is_string())
                {
                    outArgs.push_back(ReplaceVars(val.get<std::string>(), vars));
                }
                else if (val.is_array())
                {
                    for (const auto &v : val)
                    {
                        if (v.is_string())
                            outArgs.push_back(ReplaceVars(v.get<std::string>(), vars));
                    }
                }
            }
        }
    }

    std::vector<std::string> SplitWhitespaceArgs(const std::string &str)
    {
        std::vector<std::string> out;
        std::string cur;
        bool inQuotes = false;
        for (char c : str)
        {
            if (c == '"')
            {
                inQuotes = !inQuotes;
            }
            else if (std::isspace(static_cast<unsigned char>(c)) && !inQuotes)
            {
                if (!cur.empty())
                {
                    out.push_back(cur);
                    cur.clear();
                }
            }
            else
            {
                cur.push_back(c);
            }
        }
        if (!cur.empty())
            out.push_back(cur);
        return out;
    }

    bool ContainsFlag(const std::vector<std::string> &args, const std::string &prefix)
    {
        for (const auto &a : args)
        {
            if (a == prefix || a.rfind(prefix, 0) == 0)
                return true;
        }
        return false;
    }

    VersionEntry InspectLocalVersionFolder(const fs::path &dirPath)
    {
        VersionEntry entry;
        entry.id = dirPath.filename().string();
        entry.type = "release";
        entry.isDownloaded = true;

        fs::path jsonPath = dirPath / (entry.id + ".json");
        std::error_code ec;
        if (fs::exists(jsonPath, ec))
        {
            std::ifstream in(jsonPath);
            if (in.is_open())
            {
                json j = json::parse(in, nullptr, false);
                if (j.is_object())
                {
                    entry.type = j.value("type", "release");
                    entry.inheritsFrom = j.value("inheritsFrom", "");
                    std::string mainClass = ToLowerAscii(j.value("mainClass", ""));
                    std::string lowerId = ToLowerAscii(entry.id);

                    if (lowerId.find("neoforge") != std::string::npos || mainClass.find("neoforge") != std::string::npos)
                    {
                        entry.isModded = true;
                        entry.loaderName = "NeoForge";
                    }
                    else if (lowerId.find("forge") != std::string::npos ||
                             mainClass.find("minecraftforge") != std::string::npos ||
                             mainClass.find("bootstraplauncher") != std::string::npos ||
                             mainClass.find("modlauncher") != std::string::npos ||
                             mainClass.find("launchwrapper") != std::string::npos)
                    {
                        entry.isModded = true;
                        entry.loaderName = "Forge";
                    }
                    else if (lowerId.find("fabric") != std::string::npos || mainClass.find("fabricmc") != std::string::npos)
                    {
                        entry.isModded = true;
                        entry.loaderName = "Fabric";
                    }
                    else if (!entry.inheritsFrom.empty())
                    {
                        entry.isModded = true;
                        entry.loaderName = "Modded";
                    }
                }
            }
        }
        return entry;
    }

    std::vector<VersionEntry> ParseManifestVersions(
        const json &manifestJson,
        const std::unordered_map<std::string, VersionEntry> &localMap,
        bool includeSnapshots)
    {
        std::vector<VersionEntry> result;
        std::set<std::string> addedIds;

        // Place installed modded/Forge versions at the very top so they are immediately accessible
        for (const auto &[id, localEntry] : localMap)
        {
            if (localEntry.isModded)
            {
                result.push_back(localEntry);
                addedIds.insert(id);
            }
        }

        if (manifestJson.is_object() && manifestJson.contains("versions") && manifestJson["versions"].is_array())
        {
            for (const auto &ver : manifestJson["versions"])
            {
                if (!ver.is_object())
                    continue;
                std::string verType = ver.value("type", "release");
                if (verType != "release" && !(includeSnapshots && verType == "snapshot"))
                    continue;

                std::string verId = ver.value("id", "");
                if (verId.empty() || addedIds.count(verId))
                    continue;

                VersionEntry e;
                e.id = verId;
                e.type = verType;
                e.url = ver.value("url", "");
                auto it = localMap.find(verId);
                if (it != localMap.end())
                {
                    e.isDownloaded = true;
                    e.isModded = it->second.isModded;
                    e.loaderName = it->second.loaderName;
                    e.inheritsFrom = it->second.inheritsFrom;
                }
                result.push_back(std::move(e));
                addedIds.insert(verId);
            }
        }

        // Also include any remaining local-only custom versions
        for (const auto &[id, localEntry] : localMap)
        {
            if (!addedIds.count(id))
            {
                result.push_back(localEntry);
                addedIds.insert(id);
            }
        }

        return result;
    }
} // namespace

// --- Microsoft Authentication ---

bool MicrosoftAuth::RequestDeviceCode(MSADeviceCode &outCode)
{
    httplib::SSLClient cli("login.microsoftonline.com");
    cli.enable_server_certificate_verification(false);
    cli.set_connection_timeout(10, 0);

    std::string body = "client_id=00000000402b5328&scope=XboxLive.signin%20offline_access";
    auto res = cli.Post("/consumers/oauth2/v2.0/devicecode", body, "application/x-www-form-urlencoded");

    if (res && res->status == 200)
    {
        json j = json::parse(res->body, nullptr, false);
        if (!j.is_object())
            return false;
        outCode.userCode = j.value("user_code", "");
        outCode.deviceCode = j.value("device_code", "");
        outCode.verificationUri = j.value("verification_uri", "https://microsoft.com/link");
        outCode.interval = j.value("interval", 5);
        outCode.expiresIn = j.value("expires_in", 900);
        return !outCode.deviceCode.empty();
    }
    return false;
}

bool MicrosoftAuth::AuthenticateWithDeviceCode(
    const MSADeviceCode &code,
    PlayerProfile &outProfile,
    std::atomic<bool> &cancelFlag,
    std::string &statusOut)
{
    std::string msaAccessToken;

    int elapsed = 0;
    while (elapsed < code.expiresIn && !cancelFlag)
    {
        std::this_thread::sleep_for(std::chrono::seconds(code.interval));
        if (cancelFlag)
            break;
        elapsed += code.interval;

        httplib::SSLClient cli("login.microsoftonline.com");
        cli.enable_server_certificate_verification(false);

        std::string body = "grant_type=urn:ietf:params:oauth:grant-type:device_code&client_id=00000000402b5328&device_code=" + code.deviceCode;
        auto res = cli.Post("/consumers/oauth2/v2.0/token", body, "application/x-www-form-urlencoded");

        if (res && res->status == 200)
        {
            json j = json::parse(res->body, nullptr, false);
            if (j.contains("access_token"))
            {
                msaAccessToken = j["access_token"].get<std::string>();
                break;
            }
        }
        else if (res && res->status == 400)
        {
            json j = json::parse(res->body, nullptr, false);
            std::string err = j.value("error", "");
            if (err != "authorization_pending")
            {
                statusOut = "Microsoft Login Error: " + err;
                return false;
            }
        }
    }

    if (msaAccessToken.empty())
    {
        statusOut = cancelFlag ? "Authentication cancelled." : "Login timed out.";
        return false;
    }

    statusOut = "Authenticating with Xbox Live...";

    std::string xblToken;
    std::string userHash;
    {
        httplib::SSLClient cli("user.auth.xboxlive.com");
        cli.enable_server_certificate_verification(false);

        json body = {
            {"Properties", {
                {"AuthMethod", "RPS"},
                {"SiteName", "user.auth.xboxlive.com"},
                {"RpsTicket", "d=" + msaAccessToken}
            }},
            {"RelyingParty", "http://auth.xboxlive.com"},
            {"TokenType", "JWT"}
        };

        auto res = cli.Post("/user/authenticate", body.dump(), "application/json");
        if (res && res->status == 200)
        {
            json j = json::parse(res->body, nullptr, false);
            xblToken = j.value("Token", "");
            if (j.contains("DisplayClaims") && j["DisplayClaims"].contains("xui") && !j["DisplayClaims"]["xui"].empty())
            {
                userHash = j["DisplayClaims"]["xui"][0].value("uhs", "");
            }
        }
    }

    if (xblToken.empty() || userHash.empty())
    {
        statusOut = "Failed Xbox Live Authentication.";
        return false;
    }

    statusOut = "Requesting XSTS Security Token...";

    std::string xstsToken;
    {
        httplib::SSLClient cli("xsts.auth.xboxlive.com");
        cli.enable_server_certificate_verification(false);

        json body = {
            {"Properties", {
                {"SandboxId", "RETAIL"},
                {"UserTokens", {xblToken}}
            }},
            {"RelyingParty", "rp://api.minecraftservices.com/"},
            {"TokenType", "JWT"}
        };

        auto res = cli.Post("/xsts/authorize", body.dump(), "application/json");
        if (res && res->status == 200)
        {
            json j = json::parse(res->body, nullptr, false);
            xstsToken = j.value("Token", "");
        }
        else if (res && res->status == 401)
        {
            json j = json::parse(res->body, nullptr, false);
            uint64_t errCode = j.value("XErr", 0ULL);
            if (errCode == 2148916233ULL)
                statusOut = "Account has no Xbox profile.";
            else if (errCode == 2148916238ULL)
                statusOut = "Child account requires family approval.";
            else
                statusOut = "XSTS authorization error.";
            return false;
        }
    }

    if (xstsToken.empty())
    {
        statusOut = "Failed XSTS authorization.";
        return false;
    }

    statusOut = "Logging into Minecraft Services...";

    std::string mcAccessToken;
    {
        httplib::SSLClient cli("api.minecraftservices.com");
        cli.enable_server_certificate_verification(false);

        json body = {
            {"identityToken", "XBL3.0 x=" + userHash + ";" + xstsToken}
        };

        auto res = cli.Post("/authentication/login_with_xbox", body.dump(), "application/json");
        if (res && res->status == 200)
        {
            json j = json::parse(res->body, nullptr, false);
            mcAccessToken = j.value("access_token", "");
        }
    }

    if (mcAccessToken.empty())
    {
        statusOut = "Minecraft authentication failed.";
        return false;
    }

    statusOut = "Fetching Minecraft Profile...";

    {
        httplib::SSLClient cli("api.minecraftservices.com");
        cli.enable_server_certificate_verification(false);

        httplib::Headers headers = {{"Authorization", "Bearer " + mcAccessToken}};
        auto res = cli.Get("/minecraft/profile", headers);
        if (res && res->status == 200)
        {
            json j = json::parse(res->body, nullptr, false);
            std::string rawUuid = j.value("id", "");
            std::string name = j.value("name", "");

            std::string formattedUuid = rawUuid;
            if (rawUuid.length() == 32)
            {
                formattedUuid = rawUuid.substr(0, 8) + "-" +
                                rawUuid.substr(8, 4) + "-" +
                                rawUuid.substr(12, 4) + "-" +
                                rawUuid.substr(16, 4) + "-" +
                                rawUuid.substr(20);
            }

            outProfile.username = name;
            outProfile.uuid = formattedUuid;
            outProfile.accessToken = mcAccessToken;
            outProfile.userType = "msa";
            outProfile.isOnline = true;

            statusOut = "Logged in as " + name;
            return true;
        }
        statusOut = "Minecraft account does not own Minecraft!";
        return false;
    }
}

// --- MinecraftLauncher Implementation ---

std::string MinecraftLauncher::GetDefaultMinecraftDir()
{
    if (const char *appDataEnv = std::getenv("APPDATA"))
    {
        return std::string(appDataEnv) + "\\.minecraft";
    }
    return ".minecraft";
}

std::string MinecraftLauncher::CalculateSHA1(const std::string &filepath)
{
    HANDLE hFile = CreateFileA(
        filepath.c_str(), GENERIC_READ, FILE_SHARE_READ,
        nullptr, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, nullptr);
    if (hFile == INVALID_HANDLE_VALUE)
        return "";

    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[20] = {0};
    DWORD cbHash = 20;

    if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_SHA1, 0, 0, &hHash))
        {
            BYTE buffer[65536];
            DWORD bytesRead = 0;
            while (ReadFile(hFile, buffer, sizeof(buffer), &bytesRead, nullptr) && bytesRead > 0)
            {
                CryptHashData(hHash, buffer, bytesRead, 0);
            }
            CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }
    CloseHandle(hFile);

    char hashStr[41];
    snprintf(hashStr, sizeof(hashStr),
             "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
             rgbHash[0], rgbHash[1], rgbHash[2], rgbHash[3], rgbHash[4],
             rgbHash[5], rgbHash[6], rgbHash[7], rgbHash[8], rgbHash[9],
             rgbHash[10], rgbHash[11], rgbHash[12], rgbHash[13], rgbHash[14],
             rgbHash[15], rgbHash[16], rgbHash[17], rgbHash[18], rgbHash[19]);

    return std::string(hashStr);
}

bool MinecraftLauncher::IsFileValid(const std::string &filepath, size_t expectedSize, const std::string &expectedHash)
{
    std::error_code ec;
    if (!fs::exists(filepath, ec) || fs::is_directory(filepath, ec))
        return false;

    uint64_t actualSize = fs::file_size(filepath, ec);
    if (ec || actualSize == 0)
        return false;
    if (expectedSize > 0 && actualSize != expectedSize)
        return false;

    if (!expectedHash.empty())
    {
        std::string actualHash = CalculateSHA1(filepath);
        if (actualHash != ToLowerAscii(expectedHash))
            return false;
    }
    return true;
}

bool MinecraftLauncher::DownloadString(const std::string &url, std::string &outBody)
{
    return HttpGetBodyInternal(url, outBody);
}

bool MinecraftLauncher::DownloadFile(const std::string &url, const std::string &savePath)
{
    if (url.empty() || savePath.empty())
        return false;

    std::error_code ec;
    fs::path dstPath(savePath);
    fs::create_directories(dstPath.parent_path(), ec);
    fs::path tmpPath(savePath + ".part");

    std::string body;
    if (HttpGetBodyInternal(url, body) && !body.empty())
    {
        std::ofstream out(tmpPath, std::ios::binary | std::ios::trunc);
        if (out.is_open())
        {
            out.write(body.data(), static_cast<std::streamsize>(body.size()));
            out.close();
            fs::rename(tmpPath, dstPath, ec);
            if (!ec)
                return true;
            fs::copy_file(tmpPath, dstPath, fs::copy_options::overwrite_existing, ec);
            fs::remove(tmpPath, ec);
            if (!ec)
                return true;
        }
    }

    // Fallback to WinINet URLDownloadToFileA
    HRESULT hr = URLDownloadToFileA(nullptr, url.c_str(), tmpPath.string().c_str(), 0, nullptr);
    if (SUCCEEDED(hr) && fs::exists(tmpPath, ec) && fs::file_size(tmpPath, ec) > 0)
    {
        fs::rename(tmpPath, dstPath, ec);
        if (!ec)
            return true;
    }

    fs::remove(tmpPath, ec);
    return false;
}

std::string MinecraftLauncher::MavenCoordToRelativePath(const std::string &coord)
{
    if (coord.empty())
        return "";

    std::string ext = "jar";
    std::string baseCoord = coord;
    size_t atPos = baseCoord.find('@');
    if (atPos != std::string::npos)
    {
        ext = baseCoord.substr(atPos + 1);
        baseCoord = baseCoord.substr(0, atPos);
    }

    std::vector<std::string> parts;
    std::stringstream ss(baseCoord);
    std::string item;
    while (std::getline(ss, item, ':'))
    {
        parts.push_back(item);
    }
    if (parts.size() < 3)
        return "";

    std::string groupPath = parts[0];
    std::replace(groupPath.begin(), groupPath.end(), '.', '/');
    const std::string &artifact = parts[1];
    const std::string &version = parts[2];

    std::string fileName = artifact + "-" + version;
    if (parts.size() >= 4 && !parts[3].empty())
        fileName += "-" + parts[3];
    fileName += "." + ext;

    return groupPath + "/" + artifact + "/" + version + "/" + fileName;
}

std::string MinecraftLauncher::GenerateOfflineUUID(const std::string &username)
{
    std::string input = "OfflinePlayer:" + username;
    HCRYPTPROV hProv = 0;
    HCRYPTHASH hHash = 0;
    BYTE rgbHash[16] = {0};
    DWORD cbHash = 16;

    if (CryptAcquireContextA(&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
    {
        if (CryptCreateHash(hProv, CALG_MD5, 0, 0, &hHash))
        {
            CryptHashData(hHash, reinterpret_cast<const BYTE *>(input.c_str()), static_cast<DWORD>(input.length()), 0);
            CryptGetHashParam(hHash, HP_HASHVAL, rgbHash, &cbHash, 0);
            CryptDestroyHash(hHash);
        }
        CryptReleaseContext(hProv, 0);
    }

    rgbHash[6] = (rgbHash[6] & 0x0f) | 0x30;
    rgbHash[8] = (rgbHash[8] & 0x3f) | 0x80;

    char uuidStr[37];
    snprintf(uuidStr, sizeof(uuidStr),
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             rgbHash[0], rgbHash[1], rgbHash[2], rgbHash[3],
             rgbHash[4], rgbHash[5],
             rgbHash[6], rgbHash[7],
             rgbHash[8], rgbHash[9],
             rgbHash[10], rgbHash[11], rgbHash[12], rgbHash[13], rgbHash[14], rgbHash[15]);

    return std::string(uuidStr);
}

PlayerProfile MinecraftLauncher::CreateOfflineProfile(const std::string &username)
{
    PlayerProfile profile;
    profile.username = username.empty() ? "Steve" : username;
    profile.uuid = GenerateOfflineUUID(profile.username);
    profile.accessToken = "0";
    profile.userType = "legacy";
    profile.isOnline = false;
    return profile;
}

static std::unordered_map<std::string, VersionEntry> ScanLocalVersionEntries(const std::string &gameDir)
{
    std::unordered_map<std::string, VersionEntry> localMap;
    std::error_code ec;
    fs::path verDir = fs::path(gameDir) / "versions";
    if (!fs::exists(verDir, ec))
        return localMap;

    for (const auto &entry : fs::directory_iterator(verDir, ec))
    {
        if (!entry.is_directory(ec))
            continue;
        std::string verName = entry.path().filename().string();
        if (fs::exists(entry.path() / (verName + ".json"), ec) ||
            fs::exists(entry.path() / (verName + ".jar"), ec))
        {
            localMap[verName] = InspectLocalVersionFolder(entry.path());
        }
    }
    return localMap;
}

std::vector<VersionEntry> MinecraftLauncher::LoadCachedVersions(const std::string &gameDir, bool includeSnapshots)
{
    auto localMap = ScanLocalVersionEntries(gameDir);
    fs::path cacheFile = fs::path(gameDir) / "launcher_cache" / "version_manifest_v2.json";

    json manifestJson;
    std::error_code ec;
    if (fs::exists(cacheFile, ec))
    {
        std::ifstream in(cacheFile);
        if (in.is_open())
            manifestJson = json::parse(in, nullptr, false);
    }

    return ParseManifestVersions(manifestJson, localMap, includeSnapshots);
}

std::vector<VersionEntry> MinecraftLauncher::FetchVersions(const std::string &gameDir, bool includeSnapshots)
{
    auto localMap = ScanLocalVersionEntries(gameDir);

    std::string body;
    json manifestJson;
    if (DownloadString("https://launchermeta.mojang.com/mc/game/version_manifest_v2.json", body))
    {
        manifestJson = json::parse(body, nullptr, false);
        if (manifestJson.is_object() && manifestJson.contains("versions"))
        {
            std::error_code ec;
            fs::path cacheDir = fs::path(gameDir) / "launcher_cache";
            fs::create_directories(cacheDir, ec);
            std::ofstream out(cacheDir / "version_manifest_v2.json", std::ios::trunc);
            if (out.is_open())
                out << body;
        }
    }

    if (!manifestJson.is_object())
    {
        return LoadCachedVersions(gameDir, includeSnapshots);
    }

    return ParseManifestVersions(manifestJson, localMap, includeSnapshots);
}

bool MinecraftLauncher::ResolveVersionJson(
    const std::string &gameDir,
    const std::string &versionId,
    json &outMergedData,
    const std::string &versionUrl)
{
    fs::path jsonPath = fs::path(gameDir) / "versions" / versionId / (versionId + ".json");
    std::error_code ec;

    if (!fs::exists(jsonPath, ec))
    {
        std::string urlToUse = versionUrl;
        if (urlToUse.empty())
        {
            // Check local cache manifest first before making network call
            for (const auto &v : LoadCachedVersions(gameDir, true))
            {
                if (v.id == versionId && !v.url.empty())
                {
                    urlToUse = v.url;
                    break;
                }
            }
        }
        if (urlToUse.empty())
        {
            for (const auto &v : FetchVersions(gameDir, true))
            {
                if (v.id == versionId && !v.url.empty())
                {
                    urlToUse = v.url;
                    break;
                }
            }
        }
        if (!urlToUse.empty())
        {
            DownloadFile(urlToUse, jsonPath.string());
        }
    }

    if (!fs::exists(jsonPath, ec))
        return false;

    std::ifstream f(jsonPath);
    if (!f.is_open())
        return false;
    json child = json::parse(f, nullptr, false);
    f.close();
    if (!child.is_object())
        return false;

    if (!child.contains("inheritsFrom") || !child["inheritsFrom"].is_string() || child["inheritsFrom"].get<std::string>().empty())
    {
        outMergedData = std::move(child);
        return true;
    }

    std::string parentId = child["inheritsFrom"].get<std::string>();
    json parent;
    if (!ResolveVersionJson(gameDir, parentId, parent))
        return false;

    json merged = parent;
    merged["id"] = child.value("id", versionId);
    merged["inheritsFrom"] = parentId;
    if (child.contains("mainClass"))
        merged["mainClass"] = child["mainClass"];
    if (child.contains("type"))
        merged["type"] = child["type"];
    if (child.contains("minecraftArguments"))
        merged["minecraftArguments"] = child["minecraftArguments"];
    if (child.contains("assetIndex"))
        merged["assetIndex"] = child["assetIndex"];
    if (child.contains("assets"))
        merged["assets"] = child["assets"];
    if (child.contains("javaVersion"))
        merged["javaVersion"] = child["javaVersion"];
    if (child.contains("downloads"))
        merged["downloads"] = child["downloads"];

    // Merge libraries: child libraries override parent libraries with identical group:artifact[:classifier]
    std::vector<json> combinedLibs;
    std::set<std::string> childKeys;
    if (child.contains("libraries") && child["libraries"].is_array())
    {
        for (const auto &lib : child["libraries"])
        {
            std::string key = LibraryIdentityKey(lib);
            if (!key.empty())
                childKeys.insert(key);
            combinedLibs.push_back(lib);
        }
    }
    if (parent.contains("libraries") && parent["libraries"].is_array())
    {
        for (const auto &lib : parent["libraries"])
        {
            std::string key = LibraryIdentityKey(lib);
            if (key.empty() || !childKeys.count(key))
            {
                combinedLibs.push_back(lib);
            }
        }
    }
    merged["libraries"] = std::move(combinedLibs);

    // Merge arguments.game and arguments.jvm
    if (child.contains("arguments") && child["arguments"].is_object())
    {
        if (!merged.contains("arguments") || !merged["arguments"].is_object())
            merged["arguments"] = json::object();

        for (const char *section : {"game", "jvm"})
        {
            if (child["arguments"].contains(section) && child["arguments"][section].is_array())
            {
                if (!merged["arguments"].contains(section) || !merged["arguments"][section].is_array())
                    merged["arguments"][section] = json::array();

                for (const auto &item : child["arguments"][section])
                    merged["arguments"][section].push_back(item);
            }
        }
    }

    outMergedData = std::move(merged);
    return true;
}

bool MinecraftLauncher::EnsureVersionDownloaded(
    const std::string &gameDir,
    const std::string &versionId,
    json &outResolvedData,
    std::atomic<float> &progressOut,
    const std::string &versionUrl,
    const StatusCallback &statusCb,
    const LogCallback &logCb)
{
    auto setStatus = [&](const std::string &s) {
        if (statusCb)
            statusCb(s);
    };
    auto appendLog = [&](const std::string &s) {
        if (logCb)
            logCb(s);
    };

    setStatus("Resolving version manifest for " + versionId + "...");
    if (!ResolveVersionJson(gameDir, versionId, outResolvedData, versionUrl))
    {
        appendLog("[Launcher] Failed to resolve JSON for " + versionId);
        return false;
    }

    fs::path verDir = fs::path(gameDir) / "versions" / versionId;
    fs::path nativesDir = verDir / "natives";
    std::error_code ec;
    fs::create_directories(verDir, ec);
    fs::create_directories(nativesDir, ec);

    // Ensure client JAR (for vanilla OR parent vanilla jar when inheritsFrom is set)
    std::string jarVer = versionId;
    if (outResolvedData.contains("inheritsFrom") && outResolvedData["inheritsFrom"].is_string())
    {
        jarVer = outResolvedData["inheritsFrom"].get<std::string>();
    }
    fs::path clientJarPath = fs::path(gameDir) / "versions" / jarVer / (jarVer + ".jar");
    if (outResolvedData.contains("downloads") && outResolvedData["downloads"].contains("client"))
    {
        const auto &clientDl = outResolvedData["downloads"]["client"];
        std::string clientUrl = clientDl.value("url", "");
        size_t expSize = clientDl.value("size", static_cast<size_t>(0));
        std::string expHash = clientDl.value("sha1", "");

        if (!IsFileValid(clientJarPath.string(), expSize, expHash) && !clientUrl.empty())
        {
            setStatus("Downloading client JAR (" + jarVer + ")...");
            appendLog("[Launcher] Downloading " + clientUrl);
            if (!DownloadFile(clientUrl, clientJarPath.string()))
            {
                appendLog("[Launcher] ERROR: Failed to download client JAR.");
                return false;
            }
        }
    }

    // Collect libraries & natives
    std::vector<LibArtifactInfo> allArtifacts;
    if (outResolvedData.contains("libraries") && outResolvedData["libraries"].is_array())
    {
        for (const auto &lib : outResolvedData["libraries"])
        {
            auto collected = CollectLibraryArtifacts(lib);
            for (auto &item : collected)
                allArtifacts.push_back(std::move(item));
        }
    }

    // Parallel library verification and downloading
    struct LibTask
    {
        LibArtifactInfo info;
        std::string fullPath;
    };
    std::vector<LibTask> missingLibs;
    std::vector<LibTask> nativeLibs;

    for (const auto &art : allArtifacts)
    {
        std::string fullPath = (fs::path(gameDir) / "libraries" / ToBackslashes(art.relPath)).string();
        if (art.isNative)
            nativeLibs.push_back({art, fullPath});

        if (!IsFileValid(fullPath, art.size, art.sha1))
        {
            if (!art.url.empty())
                missingLibs.push_back({art, fullPath});
        }
    }

    if (!missingLibs.empty())
    {
        appendLog("[Launcher] Downloading " + std::to_string(missingLibs.size()) + " libraries in parallel...");
        std::atomic<size_t> nextIdx{0};
        std::atomic<size_t> doneCount{0};
        std::atomic<bool> anyFailed{false};
        const size_t total = missingLibs.size();
        const size_t workersCount = std::min<size_t>(8, std::max<size_t>(2, std::thread::hardware_concurrency()));

        std::vector<std::thread> workers;
        for (size_t t = 0; t < workersCount; ++t)
        {
            workers.emplace_back([&]() {
                while (true)
                {
                    size_t idx = nextIdx.fetch_add(1);
                    if (idx >= total)
                        break;
                    const auto &task = missingLibs[idx];
                    bool ok = DownloadFile(task.info.url, task.fullPath);
                    if (!ok && !task.info.fallbackUrl.empty())
                        ok = DownloadFile(task.info.fallbackUrl, task.fullPath);
                    if (!ok)
                        anyFailed = true;

                    size_t done = doneCount.fetch_add(1) + 1;
                    progressOut = 0.45f * (static_cast<float>(done) / static_cast<float>(total));
                    setStatus("Downloading libraries (" + std::to_string(done) + "/" + std::to_string(total) + ")...");
                }
            });
        }
        for (auto &w : workers)
            if (w.joinable()) w.join();

        if (anyFailed)
            appendLog("[Launcher] Warning: Some optional libraries failed to download.");
    }
    else
    {
        progressOut = 0.45f;
    }

    // Native extraction via native minizip-ng (skipped if already extracted cleanly)
    if (!nativeLibs.empty())
    {
        std::string stampSignature;
        for (const auto &nl : nativeLibs)
        {
            if (fs::exists(nl.fullPath, ec))
                stampSignature += nl.fullPath + ":" + std::to_string(fs::file_size(nl.fullPath, ec)) + ";";
        }
        fs::path stampPath = nativesDir / ".extracted_stamp";
        std::string existingStamp;
        if (fs::exists(stampPath, ec))
        {
            std::ifstream stIn(stampPath);
            std::getline(stIn, existingStamp, '\0');
        }

        if (existingStamp != stampSignature)
        {
            setStatus("Extracting native libraries...");
            for (const auto &nl : nativeLibs)
            {
                if (fs::exists(nl.fullPath, ec))
                    ZipUtils::ExtractZipEntries(nl.fullPath, nativesDir.string(), nl.info.excludePrefixes);
            }
            std::ofstream stOut(stampPath, std::ios::trunc);
            if (stOut.is_open())
                stOut << stampSignature;
        }
    }

    // Assets index & parallel asset objects
    if (outResolvedData.contains("assetIndex") && outResolvedData["assetIndex"].is_object())
    {
        std::string assetId = outResolvedData["assetIndex"].value("id", "");
        std::string assetUrl = outResolvedData["assetIndex"].value("url", "");
        size_t idxSize = outResolvedData["assetIndex"].value("size", static_cast<size_t>(0));
        std::string idxHash = outResolvedData["assetIndex"].value("sha1", "");

        if (!assetId.empty())
        {
            fs::path assetIndexPath = fs::path(gameDir) / "assets" / "indexes" / (assetId + ".json");
            if (!IsFileValid(assetIndexPath.string(), idxSize, idxHash) && !assetUrl.empty())
            {
                setStatus("Downloading asset index " + assetId + "...");
                DownloadFile(assetUrl, assetIndexPath.string());
            }

            if (fs::exists(assetIndexPath, ec))
            {
                std::ifstream af(assetIndexPath);
                json assetData = json::parse(af, nullptr, false);
                af.close();

                if (assetData.is_object() && assetData.contains("objects") && assetData["objects"].is_object())
                {
                    struct AssetTask
                    {
                        std::string url;
                        std::string path;
                    };
                    std::vector<AssetTask> tasks;
                    tasks.reserve(assetData["objects"].size());

                    for (auto &[key, value] : assetData["objects"].items())
                    {
                        std::string hash = value.value("hash", "");
                        if (hash.size() < 2)
                            continue;
                        std::string prefix = hash.substr(0, 2);
                        std::string objPath = (fs::path(gameDir) / "assets" / "objects" / prefix / hash).string();
                        size_t expectedSize = value.value("size", static_cast<size_t>(0));

                        if (IsFileValid(objPath, expectedSize))
                            continue;

                        tasks.push_back({
                            "https://resources.download.minecraft.net/" + prefix + "/" + hash,
                            objPath
                        });
                    }

                    if (!tasks.empty())
                    {
                        appendLog("[Launcher] Downloading " + std::to_string(tasks.size()) + " assets in parallel...");
                        std::atomic<size_t> nextIdx{0};
                        std::atomic<size_t> doneCount{0};
                        const size_t total = tasks.size();
                        const size_t workersCount = std::min<size_t>(8, std::max<size_t>(2, std::thread::hardware_concurrency()));

                        std::vector<std::thread> workers;
                        for (size_t t = 0; t < workersCount; ++t)
                        {
                            workers.emplace_back([&]() {
                                while (true)
                                {
                                    size_t idx = nextIdx.fetch_add(1);
                                    if (idx >= total)
                                        break;
                                    DownloadFile(tasks[idx].url, tasks[idx].path);

                                    size_t done = doneCount.fetch_add(1) + 1;
                                    progressOut = 0.45f + 0.55f * (static_cast<float>(done) / static_cast<float>(total));
                                    if (done % 15 == 0 || done == total)
                                    {
                                        setStatus("Downloading assets (" + std::to_string(done) + "/" + std::to_string(total) + ")...");
                                    }
                                }
                            });
                        }
                        for (auto &w : workers)
                            if (w.joinable()) w.join();
                    }
                }
            }
        }
    }

    progressOut = 1.0f;
    return true;
}

std::string MinecraftLauncher::BuildClasspath(
    const std::string &gameDir,
    const std::string &versionId,
    const json &verData)
{
    std::string classpath;
    std::set<std::string> seen;
    std::error_code ec;

    if (verData.contains("libraries") && verData["libraries"].is_array())
    {
        for (const auto &lib : verData["libraries"])
        {
            for (const auto &art : CollectLibraryArtifacts(lib))
            {
                std::string fullPath = (fs::path(gameDir) / "libraries" / ToBackslashes(art.relPath)).string();
                if (fs::exists(fullPath, ec) && !seen.count(fullPath))
                {
                    seen.insert(fullPath);
                    if (!classpath.empty())
                        classpath += ";";
                    classpath += fullPath;
                }
            }
        }
    }

    // Modern Forge (BootstrapLauncher/ModLauncher) resolves patched client jar via its libraries;
    // adding raw vanilla client jar to -cp causes split module errors on Java 17+.
    std::string mainClass = ToLowerAscii(verData.value("mainClass", ""));
    bool usesBootstrapLauncher = (mainClass.find("bootstraplauncher") != std::string::npos ||
                                  mainClass.find("modlauncher") != std::string::npos);

    fs::path ownJar = fs::path(gameDir) / "versions" / versionId / (versionId + ".jar");
    if (fs::exists(ownJar, ec))
    {
        if (!classpath.empty())
            classpath += ";";
        classpath += ownJar.string();
    }
    else if (!usesBootstrapLauncher && verData.contains("inheritsFrom") && verData["inheritsFrom"].is_string())
    {
        std::string parentId = verData["inheritsFrom"].get<std::string>();
        fs::path parentJar = fs::path(gameDir) / "versions" / parentId / (parentId + ".jar");
        if (fs::exists(parentJar, ec))
        {
            if (!classpath.empty())
                classpath += ";";
            classpath += parentJar.string();
        }
    }

    return classpath;
}

bool MinecraftLauncher::LaunchGame(
    const PlayerProfile &profile,
    const std::string &versionId,
    const LaunchOptions &options,
    std::atomic<float> &progressOut,
    const StatusCallback &statusCb,
    const LogCallback &logCb)
{
    std::string baseDir = options.baseGameDir.empty() ? GetDefaultMinecraftDir() : options.baseGameDir;
    std::string effectiveGameDir = options.instanceGameDir.empty() ? baseDir : options.instanceGameDir;
    std::error_code ec;
    fs::create_directories(effectiveGameDir, ec);
    fs::create_directories(fs::path(effectiveGameDir) / "mods", ec);

    json verData;
    if (!EnsureVersionDownloaded(baseDir, versionId, verData, progressOut, "", statusCb, logCb))
    {
        if (logCb)
            logCb("[Launcher] Could not prepare version " + versionId);
        return false;
    }

    if (statusCb)
        statusCb("Starting Minecraft " + versionId + "...");

    std::string assetsDir = (fs::path(baseDir) / "assets").string();
    std::string nativesDir = (fs::path(baseDir) / "versions" / versionId / "natives").string();
    std::string libraryDir = NormalizeSlashes((fs::path(baseDir) / "libraries").string());

    std::string assetIndexId = versionId;
    if (verData.contains("assetIndex") && verData["assetIndex"].is_object())
        assetIndexId = verData["assetIndex"].value("id", versionId);

    std::string classpath = BuildClasspath(baseDir, versionId, verData);

    // Pick appropriate Java executable based on version.json javaVersion.majorVersion
    int requiredJavaMajor = 0;
    if (verData.contains("javaVersion") && verData["javaVersion"].is_object())
    {
        requiredJavaMajor = verData["javaVersion"].value("majorVersion", 0);
    }
    else if (verData.contains("inheritsFrom"))
    {
        // Legacy Forge 1.7.10 - 1.12.2 prefers Java 8 if present
        requiredJavaMajor = 8;
    }

    auto runtimes = PlatformUtils::ScanJavaRuntimes();
    std::string javaExe = PlatformUtils::PickBestJavaExecutable(runtimes, requiredJavaMajor, options.customJavaPath);
    int chosenMajor = 0;
    for (const auto &rt : runtimes)
    {
        if (rt.path == javaExe)
        {
            chosenMajor = rt.majorVersion;
            break;
        }
    }

    std::unordered_map<std::string, std::string> vars = {
        {"auth_player_name", profile.username},
        {"version_name", versionId},
        {"game_directory", effectiveGameDir},
        {"assets_root", assetsDir},
        {"assets_index_name", assetIndexId},
        {"auth_uuid", profile.uuid},
        {"auth_access_token", profile.accessToken},
        {"clientid", "0"},
        {"auth_xuid", "0"},
        {"user_type", profile.isOnline ? "msa" : "mojang"},
        {"version_type", verData.value("type", "release")},
        {"user_properties", "{}"},
        {"natives_directory", nativesDir},
        {"launcher_name", "NolanLauncher"},
        {"launcher_version", "2.0"},
        {"classpath", classpath},
        {"classpath_separator", ";"},
        {"library_directory", libraryDir}
    };

    std::vector<std::string> jvmArgs;
    jvmArgs.push_back("-Xmx" + std::to_string(std::max(1, options.ramGB)) + "G");
    jvmArgs.push_back("-Xss1M");
    jvmArgs.push_back("-Dfile.encoding=UTF-8");
    jvmArgs.push_back("-Djava.net.preferIPv4Stack=true");
    if (chosenMajor == 0 || chosenMajor >= 16)
    {
        jvmArgs.push_back("--enable-native-access=ALL-UNNAMED");
    }

    if (!options.customJvmArgs.empty())
    {
        for (const auto &a : SplitWhitespaceArgs(options.customJvmArgs))
            jvmArgs.push_back(a);
    }

    std::vector<std::string> jsonJvmArgs;
    if (verData.contains("arguments") && verData["arguments"].contains("jvm"))
    {
        ExpandArgumentList(verData["arguments"]["jvm"], vars, jsonJvmArgs);
    }

    if (!ContainsFlag(jsonJvmArgs, "-Djava.library.path="))
        jvmArgs.push_back("-Djava.library.path=" + nativesDir);

    for (auto &a : jsonJvmArgs)
        jvmArgs.push_back(std::move(a));

    if (!ContainsFlag(jvmArgs, "-cp") && !ContainsFlag(jvmArgs, "-classpath") && !ContainsFlag(jvmArgs, "--class-path"))
    {
        jvmArgs.push_back("-cp");
        jvmArgs.push_back(classpath);
    }

    std::string mainClass = verData.value("mainClass", "net.minecraft.client.main.Main");

    std::vector<std::string> gameArgs;
    if (verData.contains("arguments") && verData["arguments"].contains("game"))
    {
        ExpandArgumentList(verData["arguments"]["game"], vars, gameArgs);
    }
    else if (verData.contains("minecraftArguments") && verData["minecraftArguments"].is_string())
    {
        for (const auto &tok : SplitWhitespaceArgs(verData["minecraftArguments"].get<std::string>()))
            gameArgs.push_back(ReplaceVars(tok, vars));
    }
    else
    {
        gameArgs = {
            "--username", profile.username,
            "--version", versionId,
            "--gameDir", effectiveGameDir,
            "--assetsDir", assetsDir,
            "--assetIndex", assetIndexId,
            "--uuid", profile.uuid,
            "--accessToken", profile.accessToken,
            "--userType", profile.isOnline ? "msa" : "mojang",
            "--versionType", verData.value("type", "release")
        };
    }

    std::string cmd = QuoteWinArg(javaExe);
    for (const auto &a : jvmArgs)
        cmd += " " + QuoteWinArg(a);
    cmd += " " + mainClass;
    for (const auto &a : gameArgs)
        cmd += " " + QuoteWinArg(a);

    if (logCb)
    {
        logCb("[Launcher] Java: " + javaExe);
        logCb("[Launcher] GameDir: " + effectiveGameDir);
        logCb("[Launcher] MainClass: " + mainClass);
    }

    if (!options.captureLogs)
    {
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi{};
        BOOL ok = CreateProcessA(
            nullptr, cmd.data(), nullptr, nullptr, FALSE,
            CREATE_NEW_CONSOLE, nullptr, effectiveGameDir.c_str(), &si, &pi);
        if (ok)
        {
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
            return true;
        }
        return false;
    }

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
    BOOL success = CreateProcessA(
        nullptr, cmd.data(), nullptr, nullptr, TRUE,
        CREATE_NO_WINDOW, nullptr, effectiveGameDir.c_str(), &si, &pi);

    CloseHandle(writePipe);

    if (!success)
    {
        CloseHandle(readPipe);
        if (logCb)
            logCb("[Launcher] ERROR: CreateProcessA failed (code " + std::to_string(GetLastError()) + ")");
        return false;
    }

    // Stream game stdout/stderr asynchronously into launcher console log
    std::thread([readPipe, hProc = pi.hProcess, hThread = pi.hThread, logCb]() {
        char buf[4096];
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
                if (!line.empty() && logCb)
                    logCb("[MC] " + line);
                pending.erase(0, pos + 1);
            }
        }
        if (!pending.empty() && logCb)
            logCb("[MC] " + pending);

        DWORD exitCode = 0;
        WaitForSingleObject(hProc, INFINITE);
        GetExitCodeProcess(hProc, &exitCode);
        if (logCb)
            logCb("[Launcher] Game process exited with code " + std::to_string(exitCode));

        CloseHandle(readPipe);
        CloseHandle(hProc);
        CloseHandle(hThread);
    }).detach();

    return true;
}
