#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace ZipUtils
{
    class ZipArchive
    {
    public:
        ZipArchive() = default;
        ~ZipArchive();

        ZipArchive(const ZipArchive &) = delete;
        ZipArchive &operator=(const ZipArchive &) = delete;

        bool Open(const std::string &zipPath);
        void Close();
        bool IsOpen() const { return m_opened; }

        bool HasEntry(const std::string &entryPath) const;
        bool ReadEntryBytes(const std::string &entryPath, std::vector<uint8_t> &outBytes) const;
        bool ReadEntryString(const std::string &entryPath, std::string &outText) const;
        bool ExtractEntryToFile(const std::string &entryPath, const std::string &destFilePath) const;

    private:
        void *m_handle = nullptr;
        bool m_opened = false;
    };

    bool ExtractZipEntries(
        const std::string &zipPath,
        const std::string &destDir,
        const std::vector<std::string> &excludePrefixes = {"META-INF/"});

    bool ReadZipEntryBytes(
        const std::string &zipPath,
        const std::string &entryPath,
        std::vector<uint8_t> &outBytes);

    bool ReadZipEntryString(
        const std::string &zipPath,
        const std::string &entryPath,
        std::string &outText);

    bool ZipHasEntry(
        const std::string &zipPath,
        const std::string &entryPath);

    bool ExtractSingleEntryToFile(
        const std::string &zipPath,
        const std::string &entryPath,
        const std::string &destFilePath);
} // namespace ZipUtils
