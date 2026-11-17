#include "launcher/zip_utils.hpp"

#include <mz.h>
#include <mz_os.h>
#include <mz_strm.h>
#include <mz_zip.h>
#include <mz_zip_rw.h>

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace fs = std::filesystem;

namespace ZipUtils
{
    namespace
    {
        std::string NormalizeZipEntryPath(std::string entry)
        {
            std::replace(entry.begin(), entry.end(), '\\', '/');
            while (!entry.empty() && entry.front() == '/')
                entry.erase(entry.begin());
            return entry;
        }

        bool IsSafeRelativePath(const std::string &relPath)
        {
            if (relPath.empty() || relPath.front() == '/' || relPath.front() == '\\')
                return false;
            if (relPath.find(':') != std::string::npos)
                return false;

            fs::path p(reinterpret_cast<const char8_t *>(relPath.c_str()));
            for (const auto &part : p)
            {
                if (part == "..")
                    return false;
            }
            return true;
        }

        bool StartsWithCaseInsensitive(const std::string &value, const std::string &prefix)
        {
            if (prefix.size() > value.size())
                return false;
            for (size_t i = 0; i < prefix.size(); ++i)
            {
                if (std::tolower(static_cast<unsigned char>(value[i])) !=
                    std::tolower(static_cast<unsigned char>(prefix[i])))
                    return false;
            }
            return true;
        }
    } // namespace

    ZipArchive::~ZipArchive()
    {
        Close();
    }

    bool ZipArchive::Open(const std::string &zipPath)
    {
        Close();
        if (!fs::exists(zipPath))
            return false;

        m_handle = mz_zip_reader_create();
        if (!m_handle)
            return false;

        if (mz_zip_reader_open_file(m_handle, zipPath.c_str()) != MZ_OK)
        {
            mz_zip_reader_delete(&m_handle);
            m_handle = nullptr;
            return false;
        }
        m_opened = true;
        return true;
    }

    void ZipArchive::Close()
    {
        if (m_handle)
        {
            if (m_opened)
                mz_zip_reader_close(m_handle);
            mz_zip_reader_delete(&m_handle);
            m_handle = nullptr;
        }
        m_opened = false;
    }

    bool ZipArchive::HasEntry(const std::string &entryPath) const
    {
        if (!m_opened || !m_handle)
            return false;
        std::string normEntry = NormalizeZipEntryPath(entryPath);
        return mz_zip_reader_locate_entry(m_handle, normEntry.c_str(), 1) == MZ_OK;
    }

    bool ZipArchive::ReadEntryBytes(const std::string &entryPath, std::vector<uint8_t> &outBytes) const
    {
        outBytes.clear();
        if (!m_opened || !m_handle)
            return false;

        std::string normEntry = NormalizeZipEntryPath(entryPath);
        if (mz_zip_reader_locate_entry(m_handle, normEntry.c_str(), 1) != MZ_OK)
            return false;

        int32_t len = mz_zip_reader_entry_save_buffer_length(m_handle);
        if (len < 0)
            return false;
        if (len == 0)
            return true;

        outBytes.resize(static_cast<size_t>(len));
        if (mz_zip_reader_entry_save_buffer(m_handle, outBytes.data(), len) != MZ_OK)
        {
            outBytes.clear();
            return false;
        }
        return true;
    }

    bool ZipArchive::ReadEntryString(const std::string &entryPath, std::string &outText) const
    {
        outText.clear();
        std::vector<uint8_t> bytes;
        if (!ReadEntryBytes(entryPath, bytes))
            return false;

        outText.assign(bytes.begin(), bytes.end());
        return true;
    }

    bool ZipArchive::ExtractEntryToFile(const std::string &entryPath, const std::string &destFilePath) const
    {
        if (!m_opened || !m_handle)
            return false;

        std::string normEntry = NormalizeZipEntryPath(entryPath);
        if (mz_zip_reader_locate_entry(m_handle, normEntry.c_str(), 1) != MZ_OK)
            return false;

        std::error_code ec;
        fs::create_directories(fs::path(destFilePath).parent_path(), ec);
        return mz_zip_reader_entry_save_file(m_handle, destFilePath.c_str()) == MZ_OK;
    }

    bool ExtractZipEntries(
        const std::string &zipPath,
        const std::string &destDir,
        const std::vector<std::string> &excludePrefixes)
    {
        if (!fs::exists(zipPath))
            return false;

        void *handle = mz_zip_reader_create();
        if (!handle)
            return false;

        if (mz_zip_reader_open_file(handle, zipPath.c_str()) != MZ_OK)
        {
            mz_zip_reader_delete(&handle);
            return false;
        }

        std::error_code ec;
        fs::create_directories(destDir, ec);

        int32_t err = mz_zip_reader_goto_first_entry(handle);
        while (err == MZ_OK)
        {
            mz_zip_file *fileInfo = nullptr;
            if (mz_zip_reader_entry_get_info(handle, &fileInfo) == MZ_OK && fileInfo && fileInfo->filename)
            {
                if (mz_zip_reader_entry_is_dir(handle) != MZ_OK)
                {
                    std::string entryName = NormalizeZipEntryPath(fileInfo->filename);
                    bool excluded = false;
                    for (const auto &prefix : excludePrefixes)
                    {
                        if (!prefix.empty() && StartsWithCaseInsensitive(entryName, NormalizeZipEntryPath(prefix)))
                        {
                            excluded = true;
                            break;
                        }
                    }

                    if (!excluded && IsSafeRelativePath(entryName))
                    {
                        fs::path outPath = fs::path(destDir) / fs::path(reinterpret_cast<const char8_t *>(entryName.c_str()));
                        fs::create_directories(outPath.parent_path(), ec);
                        mz_zip_reader_entry_save_file(handle, outPath.string().c_str());
                    }
                }
            }
            err = mz_zip_reader_goto_next_entry(handle);
        }

        mz_zip_reader_close(handle);
        mz_zip_reader_delete(&handle);
        return true;
    }

    bool ReadZipEntryBytes(
        const std::string &zipPath,
        const std::string &entryPath,
        std::vector<uint8_t> &outBytes)
    {
        ZipArchive archive;
        if (!archive.Open(zipPath))
            return false;
        return archive.ReadEntryBytes(entryPath, outBytes);
    }

    bool ReadZipEntryString(
        const std::string &zipPath,
        const std::string &entryPath,
        std::string &outText)
    {
        ZipArchive archive;
        if (!archive.Open(zipPath))
            return false;
        return archive.ReadEntryString(entryPath, outText);
    }

    bool ZipHasEntry(
        const std::string &zipPath,
        const std::string &entryPath)
    {
        ZipArchive archive;
        if (!archive.Open(zipPath))
            return false;
        return archive.HasEntry(entryPath);
    }

    bool ExtractSingleEntryToFile(
        const std::string &zipPath,
        const std::string &entryPath,
        const std::string &destFilePath)
    {
        ZipArchive archive;
        if (!archive.Open(zipPath))
            return false;
        return archive.ExtractEntryToFile(entryPath, destFilePath);
    }
} // namespace ZipUtils
