#include "platform_utils.hpp"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shobjidl.h>
#include <shellapi.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>

namespace fs = std::filesystem;

namespace PlatformUtils
{
    namespace
    {
        std::string WideToUtf8(const wchar_t *wstr)
        {
            if (!wstr || !wstr[0])
                return "";
            int len = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, nullptr, 0, nullptr, nullptr);
            if (len <= 1)
                return "";
            std::string out(static_cast<size_t>(len - 1), '\0');
            WideCharToMultiByte(CP_UTF8, 0, wstr, -1, out.data(), len, nullptr, nullptr);
            return out;
        }

        std::wstring Utf8ToWide(const std::string &str)
        {
            if (str.empty())
                return L"";
            int len = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
            if (len <= 1)
                return L"";
            std::wstring out(static_cast<size_t>(len - 1), L'\0');
            MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, out.data(), len);
            return out;
        }

        int ParseJavaMajorVersion(const std::string &verStr)
        {
            if (verStr.empty())
                return 0;
            // Handles "1.8.0_402" -> 8, "17.0.10" -> 17, "21.0.3" -> 21
            size_t dot1 = verStr.find('.');
            if (dot1 == std::string::npos)
            {
                try
                {
                    return std::stoi(verStr);
                }
                catch (...)
                {
                    return 0;
                }
            }
            try
            {
                int first = std::stoi(verStr.substr(0, dot1));
                if (first == 1)
                {
                    size_t dot2 = verStr.find('.', dot1 + 1);
                    std::string secondStr = (dot2 == std::string::npos)
                                                ? verStr.substr(dot1 + 1)
                                                : verStr.substr(dot1 + 1, dot2 - dot1 - 1);
                    return std::stoi(secondStr);
                }
                return first;
            }
            catch (...)
            {
                return 0;
            }
        }

        std::string ReadJavaVersionFromReleaseFile(const fs::path &javaExePath)
        {
            // java.exe is typically in <jdk>/bin/java.exe -> <jdk>/release
            fs::path releaseFile = javaExePath.parent_path().parent_path() / "release";
            std::error_code ec;
            if (!fs::exists(releaseFile, ec))
                return "";

            std::ifstream in(releaseFile);
            if (!in.is_open())
                return "";

            std::string line;
            while (std::getline(in, line))
            {
                if (line.rfind("JAVA_VERSION=", 0) == 0)
                {
                    std::string val = line.substr(13);
                    if (!val.empty() && val.front() == '"')
                        val.erase(val.begin());
                    if (!val.empty() && val.back() == '"')
                        val.pop_back();
                    return val;
                }
            }
            return "";
        }

        struct ScopedCom
        {
            HRESULT hr;
            ScopedCom() { hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE); }
            ~ScopedCom()
            {
                if (SUCCEEDED(hr))
                    CoUninitialize();
            }
        };
    } // namespace

    std::vector<std::string> OpenFileDialog(
        const wchar_t *title,
        const wchar_t *filterName,
        const wchar_t *filterPattern,
        bool multiSelect)
    {
        std::vector<std::string> results;
        ScopedCom com;

        IFileOpenDialog *dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return results;

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
        {
            options |= FOS_FILEMUSTEXIST | FOS_PATHMUSTEXIST;
            if (multiSelect)
                options |= FOS_ALLOWMULTISELECT;
            dialog->SetOptions(options);
        }

        if (title)
            dialog->SetTitle(title);

        if (filterName && filterPattern)
        {
            COMDLG_FILTERSPEC specs[] = {
                {filterName, filterPattern},
                {L"All Files (*.*)", L"*.*"}};
            dialog->SetFileTypes(2, specs);
            dialog->SetFileTypeIndex(1);
        }

        if (SUCCEEDED(dialog->Show(nullptr)))
        {
            if (multiSelect)
            {
                IShellItemArray *items = nullptr;
                if (SUCCEEDED(dialog->GetResults(&items)) && items)
                {
                    DWORD count = 0;
                    items->GetCount(&count);
                    for (DWORD i = 0; i < count; ++i)
                    {
                        IShellItem *item = nullptr;
                        if (SUCCEEDED(items->GetItemAt(i, &item)) && item)
                        {
                            PWSTR filePath = nullptr;
                            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)) && filePath)
                            {
                                results.push_back(WideToUtf8(filePath));
                                CoTaskMemFree(filePath);
                            }
                            item->Release();
                        }
                    }
                    items->Release();
                }
            }
            else
            {
                IShellItem *item = nullptr;
                if (SUCCEEDED(dialog->GetResult(&item)) && item)
                {
                    PWSTR filePath = nullptr;
                    if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &filePath)) && filePath)
                    {
                        results.push_back(WideToUtf8(filePath));
                        CoTaskMemFree(filePath);
                    }
                    item->Release();
                }
            }
        }

        dialog->Release();
        return results;
    }

    std::string OpenFolderDialog(const wchar_t *title, const std::string &initialDir)
    {
        std::string result;
        ScopedCom com;

        IFileOpenDialog *dialog = nullptr;
        HRESULT hr = CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_ALL, IID_PPV_ARGS(&dialog));
        if (FAILED(hr) || !dialog)
            return result;

        DWORD options = 0;
        if (SUCCEEDED(dialog->GetOptions(&options)))
        {
            dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_PATHMUSTEXIST);
        }

        if (title)
            dialog->SetTitle(title);

        if (!initialDir.empty() && fs::exists(initialDir))
        {
            IShellItem *folderItem = nullptr;
            std::wstring wDir = Utf8ToWide(initialDir);
            if (SUCCEEDED(SHCreateItemFromParsingName(wDir.c_str(), nullptr, IID_PPV_ARGS(&folderItem))) && folderItem)
            {
                dialog->SetFolder(folderItem);
                folderItem->Release();
            }
        }

        if (SUCCEEDED(dialog->Show(nullptr)))
        {
            IShellItem *item = nullptr;
            if (SUCCEEDED(dialog->GetResult(&item)) && item)
            {
                PWSTR path = nullptr;
                if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path)) && path)
                {
                    result = WideToUtf8(path);
                    CoTaskMemFree(path);
                }
                item->Release();
            }
        }

        dialog->Release();
        return result;
    }

    void OpenFolderInExplorer(const std::string &folderPath)
    {
        if (folderPath.empty())
            return;
        std::error_code ec;
        fs::create_directories(folderPath, ec);
        std::wstring wPath = Utf8ToWide(folderPath);
        ShellExecuteW(nullptr, L"open", wPath.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    void OpenUrlInBrowser(const std::string &url)
    {
        if (url.empty())
            return;
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
    }

    int GetSystemMemoryGB()
    {
        MEMORYSTATUSEX statex{};
        statex.dwLength = sizeof(statex);
        if (GlobalMemoryStatusEx(&statex))
        {
            int gb = static_cast<int>(statex.ullTotalPhys / (1024ULL * 1024ULL * 1024ULL));
            return std::max(4, gb);
        }
        return 16;
    }

    std::vector<JavaRuntimeInfo> ScanJavaRuntimes()
    {
        std::vector<JavaRuntimeInfo> runtimes;
        std::set<std::string> seenLower;

        auto tryAddJava = [&](const fs::path &javaExe) {
            std::error_code ec;
            if (!fs::exists(javaExe, ec) || fs::is_directory(javaExe, ec))
                return;

            fs::path canonical = fs::weakly_canonical(javaExe, ec);
            std::string pathStr = ec ? javaExe.string() : canonical.string();
            std::string lower = pathStr;
            std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
                return static_cast<char>(std::tolower(c));
            });

            if (seenLower.count(lower))
                return;
            seenLower.insert(lower);

            JavaRuntimeInfo info;
            info.path = pathStr;
            info.versionStr = ReadJavaVersionFromReleaseFile(javaExe);
            info.majorVersion = ParseJavaMajorVersion(info.versionStr);

            // Fallback: inspect parent folder name if release file missing (e.g. Oracle javapath)
            if (info.majorVersion == 0)
            {
                std::string parentStr = javaExe.parent_path().parent_path().filename().string();
                for (size_t i = 0; i < parentStr.size(); ++i)
                {
                    if (std::isdigit(static_cast<unsigned char>(parentStr[i])))
                    {
                        info.majorVersion = ParseJavaMajorVersion(parentStr.substr(i));
                        if (info.majorVersion > 0)
                            break;
                    }
                }
            }

            if (info.majorVersion > 0)
            {
                info.label = "Java " + std::to_string(info.majorVersion) +
                             (info.versionStr.empty() ? "" : " (" + info.versionStr + ")") +
                             " - " + info.path;
            }
            else
            {
                info.label = "System Java - " + info.path;
            }
            runtimes.push_back(std::move(info));
        };

        auto scanVendorRoot = [&](const fs::path &root) {
            std::error_code ec;
            if (!fs::exists(root, ec) || !fs::is_directory(root, ec))
                return;
            for (const auto &entry : fs::directory_iterator(root, ec))
            {
                if (entry.is_directory(ec))
                {
                    tryAddJava(entry.path() / "bin" / "java.exe");
                }
            }
        };

        // 1. JAVA_HOME
        if (const char *javaHome = std::getenv("JAVA_HOME"))
        {
            tryAddJava(fs::path(javaHome) / "bin" / "java.exe");
        }

        // 2. Common Windows JDK/JRE vendors
        const std::vector<std::string> roots = {
            "C:\\Program Files\\Java",
            "C:\\Program Files\\Eclipse Adoptium",
            "C:\\Program Files\\Microsoft",
            "C:\\Program Files\\Amazon Corretto",
            "C:\\Program Files\\Zulu",
            "C:\\Program Files\\BellSoft",
            "C:\\Program Files (x86)\\Java"
        };
        for (const auto &r : roots)
        {
            scanVendorRoot(r);
        }

        // 3. PATH lookup via SearchPathA
        char foundPath[MAX_PATH] = {0};
        if (SearchPathA(nullptr, "java.exe", nullptr, MAX_PATH, foundPath, nullptr) > 0)
        {
            tryAddJava(fs::path(foundPath));
        }

        std::sort(runtimes.begin(), runtimes.end(), [](const JavaRuntimeInfo &a, const JavaRuntimeInfo &b) {
            return a.majorVersion > b.majorVersion;
        });

        return runtimes;
    }

    std::string PickBestJavaExecutable(
        const std::vector<JavaRuntimeInfo> &runtimes,
        int requiredMajorVersion,
        const std::string &customOverridePath)
    {
        std::error_code ec;
        if (!customOverridePath.empty() && fs::exists(customOverridePath, ec))
        {
            return customOverridePath;
        }

        if (runtimes.empty())
            return "java.exe";

        if (requiredMajorVersion > 0)
        {
            // Exact match first (important for Forge 1.12.2 -> Java 8, Forge 1.18-1.20.4 -> Java 17)
            for (const auto &rt : runtimes)
            {
                if (rt.majorVersion == requiredMajorVersion)
                    return rt.path;
            }
            // Otherwise smallest runtime >= requiredMajorVersion
            const JavaRuntimeInfo *bestAbove = nullptr;
            for (const auto &rt : runtimes)
            {
                if (rt.majorVersion >= requiredMajorVersion)
                {
                    if (!bestAbove || rt.majorVersion < bestAbove->majorVersion)
                        bestAbove = &rt;
                }
            }
            if (bestAbove)
                return bestAbove->path;
        }

        return runtimes.front().path;
    }
} // namespace PlatformUtils
