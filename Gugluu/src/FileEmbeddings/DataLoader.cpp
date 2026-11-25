#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/FileEmbeddings/DataLoader.h"

#ifdef _WIN32
#include <Windows.h>
#endif

namespace
{
    constexpr std::uint64_t c_Format1 = 1;
    constexpr std::uint64_t c_Format2 = 2;

    std::filesystem::path DataFilePath()
    {
        return UtilsGugluu::ApplicationDirectory()
            / GugluuConstants::c_DataFileName;
    }

    void AppendTokens(
        TokenizedText& a_Destination,
        const TokenizedText& a_Source)
    {
        GUGLUU_ASSERT(
            a_Source.InputIds.size() == a_Source.AttentionMask.size(),
            "Token IDs and attention mask have different sizes.");

        a_Destination.InputIds.insert(
            a_Destination.InputIds.end(),
            a_Source.InputIds.begin(),
            a_Source.InputIds.end());

        a_Destination.AttentionMask.insert(
            a_Destination.AttentionMask.end(),
            a_Source.AttentionMask.begin(),
            a_Source.AttentionMask.end());
    }
}

DataLoader::DataLoader(std::filesystem::path a_RootDirectory) :
    m_RootDirectory(std::filesystem::weakly_canonical(a_RootDirectory))
{
    GUGLUU_ASSERT(
        std::filesystem::is_directory(m_RootDirectory),
        "The indexing root is not a directory: "
        << m_RootDirectory.string());

    LoadAllowedFileTypes();
    LoadData();
}

DataLoader& DataLoader::s_GetLoader(
    const std::filesystem::path& a_RootDirectory)
{
    static DataLoader s_Loader(a_RootDirectory);
    return s_Loader;
}

void DataLoader::s_Bind(
    InitializeCallback a_Initialize,
    CreateEmbeddingsCallback a_CreateEmbeddings,
    TokenizeCallback a_Tokenize)
{
    GUGLUU_ASSERT(
        a_Initialize && a_CreateEmbeddings && a_Tokenize,
        "DataLoader received an invalid callback.");

    s_InitializeCallback = std::move(a_Initialize);
    s_CreateEmbeddingsCallback = std::move(a_CreateEmbeddings);
    s_TokenizeCallback = std::move(a_Tokenize);
}

DataLoader::Data DataLoader::s_GetData(
    const std::filesystem::path& a_RootDirectory)
{
    GUGLUU_ASSERT(
        s_InitializeCallback
        && s_CreateEmbeddingsCallback
        && s_TokenizeCallback,
        "DataLoader callbacks have not been bound.");

    DataLoader& loader = s_GetLoader(a_RootDirectory);

    loader.UpdateData();
    loader.SaveData();

    const bool embeddingsChanged = loader.m_EmbeddingsChanged;

    if (embeddingsChanged)
    {
        s_InitializeCallback(loader.m_ChunkEmbeddings.embeddings);
        loader.m_EmbeddingsChanged = false;
    }

    return
    {
        loader.m_Files,
        loader.m_ChunkEmbeddings,
        embeddingsChanged
    };
}

void DataLoader::LoadAllowedFileTypes()
{
    const std::filesystem::path fileTypesPath =
        UtilsGugluu::ResourcePath(
            GugluuConstants::c_AllowedFileTypesFileName);

    std::ifstream input(fileTypesPath);

    GUGLUU_ASSERT(
        input.is_open(),
        "Unable to open allowed file types: " << fileTypesPath.string());

    std::string extension;

    while (std::getline(input, extension))
    {
        const std::size_t first =
            extension.find_first_not_of(" \t\r\n");

        if (first == std::string::npos || extension[first] == '#')
            continue;

        const std::size_t last =
            extension.find_last_not_of(" \t\r\n");

        extension = extension.substr(first, last - first + 1);
        extension = UtilsGugluu::ToLowerCase(extension);

        const bool validExtension =
            extension.size() > 1
            && extension.front() == '.'
            && std::all_of(
                extension.begin() + 1,
                extension.end(),
                [](unsigned char character)
                {
                    return std::isalnum(character)
                        || character == '_'
                        || character == '-';
                });

        GUGLUU_ASSERT(
            validExtension,
            "Invalid file extension in "
            << fileTypesPath.string() << ": " << extension);

        if (std::find(
            m_AllowedFileTypes.begin(),
            m_AllowedFileTypes.end(),
            extension) == m_AllowedFileTypes.end())
        {
            m_AllowedFileTypes.emplace_back(std::move(extension));
        }
    }

    GUGLUU_ASSERT(!input.bad(),
        "Failed while reading allowed file types.");

    GUGLUU_ASSERT(
        !m_AllowedFileTypes.empty(),
        "No allowed file types are configured in "
        << fileTypesPath.string());
}

bool DataLoader::IsAllowedFileType(
    const std::filesystem::path& a_FilePath) const
{
    const std::string extension =
        UtilsGugluu::ToLowerCase(a_FilePath.extension().string());

    return std::find(
        m_AllowedFileTypes.begin(),
        m_AllowedFileTypes.end(),
        extension) != m_AllowedFileTypes.end();
}

void DataLoader::ReplaceChunks(
    FileRecord& a_Record,
    std::vector<float> a_Embeddings)
{
    constexpr std::uint64_t dimensions =
        GugluuConstants::c_EmbeddingDimensions;

    GUGLUU_ASSERT(
        a_Embeddings.size() % dimensions == 0,
        "Embedding count is not divisible by embedding dimensions.");

    const std::uint64_t oldChunkId = a_Record.GetChunkId();
    const std::uint64_t oldChunkCount = a_Record.GetChunkCount();

    GUGLUU_ASSERT(
        oldChunkId <= m_ChunkEmbeddings.owner.size()
        && oldChunkCount <= m_ChunkEmbeddings.owner.size() - oldChunkId,
        "Invalid owner range while replacing chunks.");

    GUGLUU_ASSERT(
        oldChunkId <=
        std::numeric_limits<std::size_t>::max() / dimensions,
        "Embedding offset exceeds the addressable range.");

    const std::size_t floatStart =
        static_cast<std::size_t>(oldChunkId * dimensions);

    const std::size_t oldFloatCount =
        static_cast<std::size_t>(oldChunkCount * dimensions);

    GUGLUU_ASSERT(
        floatStart <= m_ChunkEmbeddings.embeddings.size()
        && oldFloatCount
        <= m_ChunkEmbeddings.embeddings.size() - floatStart,
        "Invalid embedding range while replacing chunks.");

    const std::uint64_t newChunkCount =
        a_Embeddings.size() / dimensions;

    m_ChunkEmbeddings.embeddings.erase(
        m_ChunkEmbeddings.embeddings.begin() + floatStart,
        m_ChunkEmbeddings.embeddings.begin()
        + floatStart + oldFloatCount);

    m_ChunkEmbeddings.embeddings.insert(
        m_ChunkEmbeddings.embeddings.begin() + floatStart,
        std::make_move_iterator(a_Embeddings.begin()),
        std::make_move_iterator(a_Embeddings.end()));

    const std::size_t ownerStart =
        static_cast<std::size_t>(oldChunkId);

    m_ChunkEmbeddings.owner.erase(
        m_ChunkEmbeddings.owner.begin() + ownerStart,
        m_ChunkEmbeddings.owner.begin()
        + ownerStart + static_cast<std::size_t>(oldChunkCount));

    m_ChunkEmbeddings.owner.insert(
        m_ChunkEmbeddings.owner.begin() + ownerStart,
        static_cast<std::size_t>(newChunkCount),
        a_Record.GetId());

    a_Record.SetChunkRange(oldChunkId, newChunkCount);

    // Any non-empty old or new range means the embedding storage changed.
    if (oldChunkCount != 0 || newChunkCount != 0)
        m_EmbeddingsChanged = true;

    RecomputeChunkIds();
}

void DataLoader::RecomputeChunkIds()
{
    std::vector<FileRecord*> orderedRecords;
    orderedRecords.reserve(m_Files.size());

    for (FileRecord& record : m_Files)
        orderedRecords.emplace_back(&record);

    std::stable_sort(
        orderedRecords.begin(),
        orderedRecords.end(),
        [](const FileRecord* left, const FileRecord* right)
        {
            return left->GetChunkId() < right->GetChunkId();
        });

    std::uint64_t nextChunkId = 0;

    for (FileRecord* record : orderedRecords)
    {
        const std::uint64_t chunkCount = record->GetChunkCount();
        record->SetChunkRange(nextChunkId, chunkCount);
        nextChunkId += chunkCount;
    }

    GUGLUU_ASSERT(
        nextChunkId == m_ChunkEmbeddings.owner.size(),
        "File chunk ranges do not cover the owner array.");
}

void DataLoader::UpdateData()
{
    std::vector<std::filesystem::path> directoriesToScan =
    {
        m_RootDirectory
    };

    std::unordered_set<std::filesystem::path> validPaths;
    std::unordered_set<std::filesystem::path> seenPaths;

    // Temporary lookup index: canonical path -> index in m_Files.
    // m_Files itself remains a vector.
    std::unordered_map<std::filesystem::path, std::size_t> existingPaths;
    existingPaths.reserve(m_Files.size());

    for (std::size_t index = 0; index < m_Files.size(); ++index)
    {
        const FileRecord& record = m_Files[index];

        std::error_code canonicalError;
        const std::filesystem::path canonicalPath =
            std::filesystem::weakly_canonical(
                record.GetPath(),
                canonicalError);

        if (!canonicalError)
            existingPaths.emplace(canonicalPath, index);
    }

    TokenizedText combinedTokens;
    std::vector<PendingFile> pendingFiles;

    std::size_t skippedEntries = 0;
    std::string firstError;

    auto processFile = [&](const std::filesystem::path& filePath)
        {
            try
            {
                const std::filesystem::path canonicalPath =
                    std::filesystem::weakly_canonical(filePath);

                validPaths.insert(canonicalPath);

                if (!seenPaths.insert(canonicalPath).second)
                    return;

                const auto existingPath = existingPaths.find(canonicalPath);
                const bool isNew = existingPath == existingPaths.end();

                FileRecord* existingRecord = nullptr;

                if (!isNew)
                {
                    const std::size_t index = existingPath->second;

                    GUGLUU_ASSERT(
                        index < m_Files.size(),
                        "Existing file index is outside m_Files.");

                    existingRecord = &m_Files[index];

                    if (!existingRecord->HasChanged())
                        return;
                }

                const std::string content =
                    FileRecord::s_ReadFile(canonicalPath);

                TokenizedText tokenized =
                    s_TokenizeCallback(content);

                const std::size_t sequenceLength =
                    GugluuConstants::c_MaxSequenceLength;

                GUGLUU_ASSERT(
                    tokenized.InputIds.size() == tokenized.AttentionMask.size()
                    && tokenized.InputIds.size() % sequenceLength == 0,
                    "Tokenizer returned an invalid tokenized shape.");

                PendingFile pending;
                pending.path = canonicalPath;
                pending.isNew = isNew;
                pending.tokenChunkStart =
                    combinedTokens.InputIds.size() / sequenceLength;
                pending.tokenChunkCount =
                    tokenized.InputIds.size() / sequenceLength;

                if (isNew)
                {
                    GUGLUU_ASSERT(
                        m_NextFileId
                        != std::numeric_limits<std::uint64_t>::max(),
                        "File ID space exhausted.");

                    pending.id = m_NextFileId++;
                }
                else
                {
                    pending.id = existingRecord->GetId();
                    pending.oldChunkId = existingRecord->GetChunkId();
                }

                AppendTokens(combinedTokens, tokenized);
                pendingFiles.emplace_back(std::move(pending));
            }
            catch (const std::exception& error)
            {
                ++skippedEntries;

                if (firstError.empty())
                    firstError = filePath.string() + ": " + error.what();

                LOG_ERROR(
                    "Unable to prepare file '{}' for indexing: {}",
                    filePath.string(),
                    error.what());
            }
        };

    while (!directoriesToScan.empty())
    {
        const std::filesystem::path directoryPath =
            std::move(directoriesToScan.back());

        directoriesToScan.pop_back();

        std::error_code error;

        std::filesystem::directory_iterator iterator(
            directoryPath,
            error);

        if (error)
        {
            ++skippedEntries;

            if (firstError.empty())
                firstError =
                directoryPath.string() + ": " + error.message();

            continue;
        }

        const std::filesystem::directory_iterator end;

        while (iterator != end)
        {
            const std::filesystem::path entryPath = iterator->path();

            std::error_code statusError;
            const auto linkStatus =
                iterator->symlink_status(statusError);

            if (statusError)
            {
                ++skippedEntries;

                if (firstError.empty())
                    firstError =
                    entryPath.string() + ": " + statusError.message();
            }
            else if (std::filesystem::is_symlink(linkStatus))
            {
                std::error_code targetError;
                const bool isRegularFile =
                    iterator->is_regular_file(targetError);

                if (targetError)
                {
                    ++skippedEntries;

                    if (firstError.empty())
                        firstError =
                        entryPath.string() + ": " + targetError.message();
                }
                else if (isRegularFile && IsAllowedFileType(entryPath))
                {
                    processFile(entryPath);
                }
            }
            else if (std::filesystem::is_directory(linkStatus))
            {
                directoriesToScan.emplace_back(entryPath);
            }
            else if (
                std::filesystem::is_regular_file(linkStatus)
                && IsAllowedFileType(entryPath))
            {
                processFile(entryPath);
            }

            error.clear();
            iterator.increment(error);

            if (error)
            {
                ++skippedEntries;

                if (firstError.empty())
                    firstError =
                    directoryPath.string() + ": " + error.message();

                break;
            }
        }
    }

    // All files have been tokenized. Run the embedding pipeline once.
    std::vector<float> combinedEmbeddings;

    if (!combinedTokens.InputIds.empty())
    {
        try
        {
            combinedEmbeddings = s_CreateEmbeddingsCallback(combinedTokens);
        }
        catch (const std::exception& error)
        {
            LOG_ERROR(
                "Batch embedding generation failed; cached data was retained: {}",
                error.what());
            return;
        }

        const std::uint64_t expectedFloatCount =
            (combinedTokens.InputIds.size()
                / GugluuConstants::c_MaxSequenceLength)
            * GugluuConstants::c_EmbeddingDimensions;

        if (combinedEmbeddings.size() != expectedFloatCount)
        {
            LOG_ERROR(
                "CreateEmbeddings returned {} floats; expected {}. "
                "Cached data was retained.",
                combinedEmbeddings.size(),
                expectedFloatCount);
            return;
        }
    }

    auto getFileEmbeddings =
        [&](const PendingFile& pending) -> std::vector<float>
        {
            const std::size_t dimensions =
                GugluuConstants::c_EmbeddingDimensions;

            const std::size_t floatStart =
                static_cast<std::size_t>(
                    pending.tokenChunkStart * dimensions);

            const std::size_t floatCount =
                static_cast<std::size_t>(
                    pending.tokenChunkCount * dimensions);

            return std::vector<float>(
                combinedEmbeddings.begin() + floatStart,
                combinedEmbeddings.begin() + floatStart + floatCount);
        };

    // Replace existing files from the highest old chunk offset downwards.
    // This keeps the original offsets of not-yet-processed files valid.
    std::vector<const PendingFile*> existingUpdates;

    for (const PendingFile& pending : pendingFiles)
    {
        if (!pending.isNew)
            existingUpdates.emplace_back(&pending);
    }

    std::sort(
        existingUpdates.begin(),
        existingUpdates.end(),
        [](const PendingFile* left, const PendingFile* right)
        {
            return left->oldChunkId > right->oldChunkId;
        });

    try
    {
        for (const PendingFile* pending : existingUpdates)
        {
            const auto existingPath = existingPaths.find(pending->path);

            GUGLUU_ASSERT(
                existingPath != existingPaths.end()
                && existingPath->second < m_Files.size(),
                "An existing file record disappeared during indexing.");

            FileRecord& record = m_Files[existingPath->second];

            ReplaceChunks(
                record,
                getFileEmbeddings(*pending));

            record.RefreshMetadata();
        }

        // New records are appended after existing records have been updated.
        for (const PendingFile& pending : pendingFiles)
        {
            if (!pending.isNew)
                continue;

            FileRecord newRecord(pending.id, pending.path);

            newRecord.SetChunkRange(
                m_ChunkEmbeddings.owner.size(),
                0);

            m_Files.emplace_back(std::move(newRecord));

            ReplaceChunks(
                m_Files.back(),
                getFileEmbeddings(pending));
        }
    }
    catch (const std::exception& error)
    {
        LOG_ERROR(
            "Unable to commit newly generated embeddings: {}",
            error.what());

        // Do not remove missing-file records after a partial commit.
        return;
    }

    // Only delete records if the filesystem scan was complete.
    if (skippedEntries == 0)
    {
        // Keep pointers while removing chunks: ReplaceChunks changes chunk
        // storage and chunk IDs, but does not modify the m_Files vector.
        std::vector<std::pair<std::uint64_t, FileRecord*>> recordsToDelete;

        for (FileRecord& record : m_Files)
        {
            std::error_code canonicalError;
            const std::filesystem::path canonicalPath =
                std::filesystem::weakly_canonical(
                    record.GetPath(),
                    canonicalError);

            if (canonicalError)
            {
                ++skippedEntries;

                if (firstError.empty())
                    firstError =
                    record.GetPath().string()
                    + ": " + canonicalError.message();

                continue;
            }

            if (!validPaths.contains(canonicalPath))
                recordsToDelete.emplace_back(record.GetChunkId(), &record);
        }

        if (skippedEntries == 0)
        {
            std::sort(
                recordsToDelete.begin(),
                recordsToDelete.end(),
                [](const auto& left, const auto& right)
                {
                    return left.first > right.first;
                });

            std::unordered_set<std::uint64_t> deletedIds;
            deletedIds.reserve(recordsToDelete.size());

            // Highest chunk offsets first, so lower offsets stay valid.
            for (const auto& [chunkId, record] : recordsToDelete)
            {
                deletedIds.emplace(record->GetId());
                ReplaceChunks(*record, {});
            }

            const auto newEnd = std::remove_if(
                m_Files.begin(),
                m_Files.end(),
                [&](const FileRecord& record)
                {
                    return deletedIds.contains(record.GetId());
                });

            m_Files.erase(newEnd, m_Files.end());
        }
    }

    if (skippedEntries != 0)
    {
        LOG_ERROR(
            "Filesystem scan was incomplete; skipped {} inaccessible "
            "or changing entries. Cached records were retained. "
            "First scan error: {}",
            skippedEntries,
            firstError);
    }
}

void DataLoader::LoadData()
{
    const std::filesystem::path dataFilePath = DataFilePath();

    if (!std::filesystem::exists(dataFilePath))
        return;

    std::ifstream input(dataFilePath);

    if (!input.is_open())
    {
        throw std::runtime_error(
            "Unable to open data file for reading: "
            + dataFilePath.string());
    }

    input >> std::ws;

    if (input.peek() == std::char_traits<char>::eof())
        return;

    std::string formatName;
    std::uint64_t formatVersion = 0;

    if (!(input >> formatName >> formatVersion)
        || formatName != "GUGLUU_DATA"
        || (formatVersion != c_Format1 && formatVersion != c_Format2))
    {
        throw std::runtime_error(
            "Data file has an invalid or unsupported format.");
    }

    if (formatVersion == c_Format2)
    {
        if (!(input >> m_NextFileId))
        {
            throw std::runtime_error(
                "Format-2 data file is missing the next file ID.");
        }
    }

    using FileTime = std::filesystem::file_time_type;

    std::unordered_set<std::filesystem::path> loadedPaths;

    while (true)
    {
        input >> std::ws;

        if (input.peek() == std::char_traits<char>::eof())
            break;

        std::uint64_t id = 0;
        std::string pathString;
        std::uintmax_t fileSize = 0;
        FileTime::duration::rep modifiedTicks = 0;
        std::uint64_t chunkCount = 0;

        if (formatVersion == c_Format1)
        {
            std::uint32_t legacyId = 0;

            if (!(input
                >> legacyId
                >> std::quoted(pathString)
                >> fileSize
                >> modifiedTicks
                >> chunkCount))
            {
                throw std::runtime_error(
                    "Data file contains an invalid format-1 file record.");
            }

            id = legacyId;
        }
        else
        {
            if (!(input
                >> id
                >> std::quoted(pathString)
                >> fileSize
                >> modifiedTicks
                >> chunkCount))
            {
                throw std::runtime_error(
                    "Data file contains an invalid format-2 file record.");
            }
        }

        if (id == std::numeric_limits<std::uint64_t>::max())
        {
            throw std::runtime_error(
                "Data file contains an exhausted file ID.");
        }

        const std::uint64_t chunkId =
            m_ChunkEmbeddings.owner.size();

        for (std::uint64_t chunk = 0; chunk < chunkCount; ++chunk)
        {
            std::uint64_t dimensions = 0;

            if (!(input >> dimensions)
                || dimensions != GugluuConstants::c_EmbeddingDimensions)
            {
                throw std::runtime_error(
                    "Data file contains an invalid embedding dimension.");
            }

            for (std::uint64_t dimension = 0;
                dimension < dimensions;
                ++dimension)
            {
                float value = 0.0f;

                if (!(input >> value) || !std::isfinite(value))
                {
                    throw std::runtime_error(
                        "Data file contains an invalid or incomplete embedding.");
                }

                m_ChunkEmbeddings.embeddings.emplace_back(value);
            }

            m_ChunkEmbeddings.owner.emplace_back(id);
        }

        const std::filesystem::path filePath(pathString);

        FileRecord record(
            id,
            filePath,
            fileSize,
            FileTime(FileTime::duration(modifiedTicks)),
            chunkId,
            chunkCount);

        if (!loadedPaths.emplace(filePath).second)
        {
            throw std::runtime_error(
                "Data file contains duplicate file paths.");
        }

        m_Files.emplace_back(std::move(record));

        m_NextFileId = (std::max)(m_NextFileId, id + 1);
    }

    RecomputeChunkIds();
}

void DataLoader::SaveData() const
{
    constexpr std::uint64_t dimensions =
        GugluuConstants::c_EmbeddingDimensions;

    const std::filesystem::path dataFilePath = DataFilePath();

    std::filesystem::path temporaryPath = dataFilePath;
    temporaryPath += ".tmp";

    std::ofstream output(temporaryPath, std::ios::trunc);

    GUGLUU_ASSERT(
        output,
        "Unable to open data file for writing: "
        << temporaryPath.string());

    // Format 2 stores the next ID so deleted IDs are never reused.
    output << "GUGLUU_DATA "
        << GugluuConstants::c_DataFormatVersion
        << ' ' << m_NextFileId << '\n';

    output << std::setprecision(
        std::numeric_limits<float>::max_digits10);

    std::vector<const FileRecord*> orderedRecords;
    orderedRecords.reserve(m_Files.size());

    for (const FileRecord& record : m_Files)
        orderedRecords.emplace_back(&record);

    std::sort(
        orderedRecords.begin(),
        orderedRecords.end(),
        [](const FileRecord* left, const FileRecord* right)
        {
            return left->GetChunkId() < right->GetChunkId();
        });

    for (const FileRecord* record : orderedRecords)
    {
        output
            << record->GetId() << ' '
            << std::quoted(record->GetPath().string()) << ' '
            << record->GetFileSize() << ' '
            << record->GetLastModified().time_since_epoch().count() << ' '
            << record->GetChunkCount()
            << '\n';

        const std::uint64_t chunkId = record->GetChunkId();
        const std::uint64_t chunkCount = record->GetChunkCount();

        GUGLUU_ASSERT(
            chunkId <= m_ChunkEmbeddings.owner.size()
            && chunkCount <= m_ChunkEmbeddings.owner.size() - chunkId,
            "Invalid owner range while saving data.");

        GUGLUU_ASSERT(
            chunkId <=
            std::numeric_limits<std::size_t>::max() / dimensions,
            "Embedding offset exceeds the addressable range.");

        const std::size_t floatStart =
            static_cast<std::size_t>(chunkId * dimensions);

        const std::size_t floatCount =
            static_cast<std::size_t>(chunkCount * dimensions);

        GUGLUU_ASSERT(
            floatStart <= m_ChunkEmbeddings.embeddings.size()
            && floatCount
            <= m_ChunkEmbeddings.embeddings.size() - floatStart,
            "Invalid embedding range while saving data.");

        for (std::uint64_t chunk = 0; chunk < chunkCount; ++chunk)
        {
            const std::size_t ownerIndex =
                static_cast<std::size_t>(chunkId + chunk);

            GUGLUU_ASSERT(
                m_ChunkEmbeddings.owner[ownerIndex] == record->GetId(),
                "Embedding owner does not match its file record.");

            const std::size_t chunkStart =
                floatStart + static_cast<std::size_t>(chunk * dimensions);

            output << dimensions;

            for (std::uint64_t dimension = 0;
                dimension < dimensions;
                ++dimension)
            {
                output << ' '
                    << m_ChunkEmbeddings.embeddings[
                        chunkStart + static_cast<std::size_t>(dimension)];
            }

            output << '\n';
        }
    }

    output.flush();

    GUGLUU_ASSERT(
        output,
        "Failed while writing data file: " << temporaryPath.string());

    output.close();

    GUGLUU_ASSERT(
        output,
        "Failed while closing data file: " << temporaryPath.string());

    #ifdef _WIN32
        GUGLUU_ASSERT(
            MoveFileExW(
                temporaryPath.c_str(),
                dataFilePath.c_str(),
                MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
            "Unable to replace data file; Windows error " << GetLastError());
    #else
        std::error_code renameError;
        std::filesystem::rename(temporaryPath, dataFilePath, renameError);
        GUGLUU_ASSERT(
            !renameError,
            "Unable to replace data file: " << renameError.message());
    #endif
}
