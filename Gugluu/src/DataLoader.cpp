#include "Gugluu/src/pch.h"
#include "Gugluu/src/DataLoader.h"
#include "Gugluu/src/utils.h"

namespace
{
    std::filesystem::path DataFilePath()
    {
        return UtilsGugluu::ApplicationDirectory()
            / GugluuConstants::c_DataFileName;
    }
}

DataLoader::DataLoader(std::filesystem::path a_RootDirectory) :
    m_RootDirectory(std::filesystem::weakly_canonical(a_RootDirectory))
{
    GUGLUU_ASSERT(
        std::filesystem::is_directory(m_RootDirectory),
        "The indexing root is not a directory: " << m_RootDirectory.string());
    LoadAllowedFileTypes();
    LoadData();
}

void DataLoader::LoadAllowedFileTypes()
{
    const std::filesystem::path c_FileTypesPath =
        UtilsGugluu::ResourcePath(
            GugluuConstants::c_AllowedFileTypesFileName);
    std::ifstream input(c_FileTypesPath);
    GUGLUU_ASSERT(
        input.is_open(),
        "Unable to open allowed file types: " << c_FileTypesPath.string());

    std::string extension;
    while (std::getline(input, extension))
    {
        const std::size_t c_FirstCharacter = extension.find_first_not_of(" \t\r\n");
        if (c_FirstCharacter == std::string::npos
            || extension[c_FirstCharacter] == '#')
            continue;

        const std::size_t c_LastCharacter = extension.find_last_not_of(" \t\r\n");
        extension = extension.substr(
            c_FirstCharacter,
            c_LastCharacter - c_FirstCharacter + 1);
        extension = UtilsGugluu::ToLowerCase(extension);

        const bool c_ValidExtension = extension.size() > 1
            && extension.front() == '.'
            && std::all_of(
                extension.begin() + 1,
                extension.end(),
                [](unsigned char a_Character)
                {
                    return std::isalnum(a_Character)
                        || a_Character == '_'
                        || a_Character == '-';
                });
        GUGLUU_ASSERT(
            c_ValidExtension,
            "Invalid file extension in " << c_FileTypesPath.string()
                << ": " << extension);

        if (std::find(
            m_AllowedFileTypes.begin(), m_AllowedFileTypes.end(), extension)
            == m_AllowedFileTypes.end())
            m_AllowedFileTypes.emplace_back(std::move(extension));
    }

    GUGLUU_ASSERT(!input.bad(), "Failed while reading allowed file types.");
    GUGLUU_ASSERT(
        !m_AllowedFileTypes.empty(),
        "No allowed file types are configured in " << c_FileTypesPath.string());
}

bool DataLoader::IsAllowedFileType(const std::filesystem::path& a_FilePath) const
{
    const std::string c_Extension =
        UtilsGugluu::ToLowerCase(a_FilePath.extension().string());
    return std::find(
        m_AllowedFileTypes.begin(),
        m_AllowedFileTypes.end(),
        c_Extension) != m_AllowedFileTypes.end();
}

const std::unordered_map<std::filesystem::path, FileRecord>&
DataLoader::UpdateData()
{
    const std::filesystem::path& c_RootDirectory = m_RootDirectory;
    std::vector<std::filesystem::path> directoriesToScan = { c_RootDirectory };
    std::unordered_set<std::filesystem::path> validPaths;
    size_t skippedEntries = 0;
    std::string firstError;

    while (!directoriesToScan.empty())
    {
        const std::filesystem::path c_DirectoryPath =
            std::move(directoriesToScan.back());
        directoriesToScan.pop_back();

        std::error_code error;
        std::filesystem::directory_iterator directoryIterator(
            c_DirectoryPath,
            error);

        if (error)
        {
            ++skippedEntries;
            if (firstError.empty())
                firstError = c_DirectoryPath.string() + ": " + error.message();
            continue;
        }

        const std::filesystem::directory_iterator c_End;
        while (directoryIterator != c_End)
        {
            const std::filesystem::path c_EntryPath = directoryIterator->path();
            std::error_code statusError;
            const std::filesystem::file_status c_LinkStatus =
                directoryIterator->symlink_status(statusError);

            if (statusError)
            {
                ++skippedEntries;
                if (firstError.empty())
                    firstError = c_EntryPath.string() + ": " + statusError.message();
            }
            else if (std::filesystem::is_symlink(c_LinkStatus))
            {
                std::error_code targetError;
                const bool c_IsRegularFile =
                    directoryIterator->is_regular_file(targetError);

                if (targetError)
                {
                    ++skippedEntries;
                    if (firstError.empty())
                        firstError = c_EntryPath.string() + ": " + targetError.message();
                }
                else if (c_IsRegularFile && IsAllowedFileType(c_EntryPath))
                {
                    try
                    {
                        auto [fileRecord, inserted] =
                            m_Files.try_emplace(c_EntryPath, c_EntryPath);

                        if (!inserted)
                            fileRecord->second.OnUpdate();
                        validPaths.insert(c_EntryPath);
                    }
                    catch (const std::filesystem::filesystem_error& fileError)
                    {
                        ++skippedEntries;
                        if (firstError.empty())
                            firstError = fileError.what();
                    }
                }
            }
            else if (std::filesystem::is_directory(c_LinkStatus))
            {
                directoriesToScan.push_back(c_EntryPath);
            }
            else if (std::filesystem::is_regular_file(c_LinkStatus)
                && IsAllowedFileType(c_EntryPath))
            {
                try
                {
                    auto [fileRecord, inserted] =
                        m_Files.try_emplace(c_EntryPath, c_EntryPath);

                    if (!inserted)
                        fileRecord->second.OnUpdate();
                    validPaths.insert(c_EntryPath);
                }
                catch (const std::filesystem::filesystem_error& fileError)
                {
                    ++skippedEntries;
                    if (firstError.empty())
                        firstError = fileError.what();
                }
            }

            error.clear();
            directoryIterator.increment(error);
            if (error)
            {
                ++skippedEntries;
                if (firstError.empty())
                    firstError = c_DirectoryPath.string() + ": " + error.message();
                break;
            }
        }
    }

    if (skippedEntries == 0)
    {
        for (auto iterator = m_Files.begin();
            iterator != m_Files.end();)
        {
            if (!validPaths.contains(iterator->first))
            {
                iterator = m_Files.erase(iterator);
            }
            else
            {
                ++iterator;
            }
        }
    }
    else
    {
        std::cerr
            << "Filesystem scan was incomplete; skipped "
            << skippedEntries
            << " inaccessible or changing entries. Cached records were retained."
            << std::endl
            << "First scan error: " << firstError << std::endl;
    }

    return m_Files;
}

void DataLoader::LoadData()
{
    const std::filesystem::path c_DataFilePath = DataFilePath();
    std::ifstream input(c_DataFilePath);

    if (!input)
    {
        if (!std::filesystem::exists(c_DataFilePath))
            return;

        throw std::runtime_error(
            "Unable to open data file for reading: " + c_DataFilePath.string());
    }

    input >> std::ws;

    if (input.peek() == std::char_traits<char>::eof())
        return;

    std::string formatName;
    unsigned int formatVersion = 0;

    if (!(input >> formatName >> formatVersion)
        || formatName != "GUGLUU_DATA"
        || formatVersion != GugluuConstants::c_DataFormatVersion)
    {
        throw std::runtime_error(
            "Data file has an invalid or unsupported format");
    }

    using FileTime = std::filesystem::file_time_type;
    const std::uintmax_t c_DataFileSize =
        std::filesystem::file_size(c_DataFilePath);

    while (true)
    {
        input >> std::ws;

        if (input.peek() == std::char_traits<char>::eof())
            break;

        uint32_t id = 0;
        std::string pathString;
        uintmax_t fileSize = 0;
        FileTime::duration::rep modifiedTicks = 0;
        size_t embeddingCount = 0;

        if (!(input
            >> id
            >> std::quoted(pathString)
            >> fileSize
            >> modifiedTicks
            >> embeddingCount)
            || embeddingCount == 0
            || embeddingCount > c_DataFileSize
                / GugluuConstants::c_EmbeddingDimensions)
        {
            throw std::runtime_error(
                "Data file contains an invalid file record");
        }

        std::vector<std::vector<float>> embeddings;

        embeddings.reserve(embeddingCount);

        for (size_t i = 0; i < embeddingCount; ++i)
        {
            std::size_t dimensionCount = 0;

            if (!(input >> dimensionCount)
                || dimensionCount != GugluuConstants::c_EmbeddingDimensions)
            {
                throw std::runtime_error(
                    "Data file contains an invalid embedding");
            }

            std::vector<float> embedding(dimensionCount);

            for (float& value : embedding)
            {
                if (!(input >> value) || !std::isfinite(value))
                {
                    throw std::runtime_error(
                        "Data file contains an invalid or incomplete embedding");
                }
            }

            embeddings.emplace_back(std::move(embedding));
        }

        const std::filesystem::path c_FilePath(pathString);

        FileRecord record(
            id,
            c_FilePath,
            fileSize,
            FileTime(
                FileTime::duration(modifiedTicks)),
            std::move(embeddings));

        m_Files.insert_or_assign(
            c_FilePath,
            std::move(record));
    }
}

const std::unordered_map<std::filesystem::path, FileRecord>&
DataLoader::GetData(const std::filesystem::path& a_RootDirectory)
{
    static DataLoader s_Loader(a_RootDirectory);
    const auto& c_Records = s_Loader.UpdateData();
    s_Loader.SaveData();
    return c_Records;
}

void DataLoader::SaveData() const
{
    const std::filesystem::path c_DataFilePath = DataFilePath();
    std::filesystem::path temporaryPath = c_DataFilePath;
    temporaryPath += L".tmp";
    std::ofstream output(temporaryPath, std::ios::trunc);

    GUGLUU_ASSERT(
        output,
        "Unable to open data file for writing: " << temporaryPath.string());

    output << "GUGLUU_DATA " << GugluuConstants::c_DataFormatVersion << '\n';

    output << std::setprecision(
        std::numeric_limits<float>::max_digits10);

    for (const auto& [path, record] : m_Files)
    {
        output
            << record.GetId() << ' '
            << std::quoted(record.GetPath().string()) << ' '
            << record.GetFileSize() << ' '
            << record.GetLastModified().time_since_epoch().count() << ' '
            << record.GetEmbeddings().size()
            << '\n';

        for (const auto& embedding : record.GetEmbeddings())
        {
            output << embedding.size();

            for (float value : embedding)
                output << ' ' << value;

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
    GUGLUU_ASSERT(
        MoveFileExW(
            temporaryPath.c_str(),
            c_DataFilePath.c_str(),
            MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH),
        "Unable to replace data file; Windows error " << GetLastError());
}