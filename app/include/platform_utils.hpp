#pragma once

#include <string>
#include <vector>

struct JavaRuntimeInfo
{
    std::string path;
    std::string versionStr;
    std::string label;
    int majorVersion = 0;
};

namespace PlatformUtils
{
    std::vector<std::string> OpenFileDialog(
        const wchar_t *title,
        const wchar_t *filterName,
        const wchar_t *filterPattern,
        bool multiSelect = false);

    std::string OpenFolderDialog(const wchar_t *title, const std::string &initialDir = "");
    void OpenFolderInExplorer(const std::string &folderPath);
    void OpenUrlInBrowser(const std::string &url);
    int GetSystemMemoryGB();

    std::vector<JavaRuntimeInfo> ScanJavaRuntimes();
    std::string PickBestJavaExecutable(
        const std::vector<JavaRuntimeInfo> &runtimes,
        int requiredMajorVersion,
        const std::string &customOverridePath);
} // namespace PlatformUtils
