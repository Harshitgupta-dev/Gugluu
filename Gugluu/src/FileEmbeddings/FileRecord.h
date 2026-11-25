#pragma once

#include "Gugluu/src/core/pch.h"

class FileRecord
{
public:
    FileRecord(
        std::uint64_t a_Id,
        std::filesystem::path a_FilePath);

    FileRecord(
        std::uint64_t a_Id,
        std::filesystem::path a_FilePath,
        std::uintmax_t a_FileSize,
        std::filesystem::file_time_type a_LastModified,
        std::uint64_t a_ChunkId,
        std::uint64_t a_ChunkCount);

    bool HasChanged() const;
    void RefreshMetadata();

    std::uint64_t GetId() const noexcept;
    const std::filesystem::path& GetPath() const noexcept;
    std::uintmax_t GetFileSize() const noexcept;
    std::filesystem::file_time_type GetLastModified() const noexcept;

    std::uint64_t GetChunkId() const noexcept;
    std::uint64_t GetChunkCount() const noexcept;
    void SetChunkRange(
        std::uint64_t a_ChunkId,
        std::uint64_t a_ChunkCount) noexcept;

    static std::string s_ReadFile(
        const std::filesystem::path& a_FilePath);

private:
    std::uint64_t m_Id = 0;
    std::filesystem::path m_Path;
    std::uintmax_t m_FileSize = 0;
    std::filesystem::file_time_type m_LastModified{};
    std::uint64_t m_ChunkId = 0;
    std::uint64_t m_ChunkCount = 0;
};
