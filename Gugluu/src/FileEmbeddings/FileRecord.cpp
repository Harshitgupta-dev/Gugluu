#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/FileEmbeddings/FileRecord.h"

FileRecord::FileRecord(
    std::uint64_t a_Id,
    std::filesystem::path a_FilePath) :
    m_Id(a_Id),
    m_Path(std::move(a_FilePath)),
    m_FileSize(std::filesystem::file_size(m_Path)),
    m_LastModified(std::filesystem::last_write_time(m_Path))
{
}

FileRecord::FileRecord(
    std::uint64_t a_Id,
    std::filesystem::path a_FilePath,
    std::uintmax_t a_FileSize,
    std::filesystem::file_time_type a_LastModified,
    std::uint64_t a_ChunkId,
    std::uint64_t a_ChunkCount) :
    m_Id(a_Id),
    m_Path(std::move(a_FilePath)),
    m_FileSize(a_FileSize),
    m_LastModified(a_LastModified),
    m_ChunkId(a_ChunkId),
    m_ChunkCount(a_ChunkCount)
{
}

bool FileRecord::HasChanged() const
{
    return std::filesystem::file_size(m_Path) != m_FileSize
        || std::filesystem::last_write_time(m_Path) != m_LastModified;
}

void FileRecord::RefreshMetadata()
{
    m_FileSize = std::filesystem::file_size(m_Path);
    m_LastModified = std::filesystem::last_write_time(m_Path);
}

std::uint64_t FileRecord::GetId() const noexcept
{
    return m_Id;
}

const std::filesystem::path& FileRecord::GetPath() const noexcept
{
    return m_Path;
}

std::uintmax_t FileRecord::GetFileSize() const noexcept
{
    return m_FileSize;
}

std::filesystem::file_time_type FileRecord::GetLastModified() const noexcept
{
    return m_LastModified;
}

std::uint64_t FileRecord::GetChunkId() const noexcept
{
    return m_ChunkId;
}

std::uint64_t FileRecord::GetChunkCount() const noexcept
{
    return m_ChunkCount;
}

void FileRecord::SetChunkRange(
    std::uint64_t a_ChunkId,
    std::uint64_t a_ChunkCount) noexcept
{
    m_ChunkId = a_ChunkId;
    m_ChunkCount = a_ChunkCount;
}

std::string FileRecord::s_ReadFile(
    const std::filesystem::path& a_FilePath)
{
    std::ifstream file(a_FilePath, std::ios::binary);

    GUGLUU_ASSERT(
        file.is_open(),
        "Unable to open file for reading: " << a_FilePath.string());

    const std::istreambuf_iterator<char> begin(file);
    const std::istreambuf_iterator<char> end;

    std::string content(begin, end);

    GUGLUU_ASSERT(
        !file.bad(),
        "Unable to finish reading file: " << a_FilePath.string());

    return content;
}
