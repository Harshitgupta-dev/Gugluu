#pragma once

#include <filesystem>
#include <string>
#include <unordered_map>
#include <vector>

#include "Gugluu/src/FileRecord.h"

class DataLoader
{
public:
    static const std::unordered_map<std::filesystem::path, FileRecord>& GetData(
        const std::filesystem::path& a_RootDirectory);

private:
    explicit DataLoader(std::filesystem::path a_RootDirectory);

    const std::unordered_map<std::filesystem::path, FileRecord>& UpdateData();
    void LoadAllowedFileTypes();
    void LoadData();
    void SaveData() const;
    bool IsAllowedFileType(const std::filesystem::path& a_FilePath) const;

    std::filesystem::path m_RootDirectory;
    std::unordered_map<std::filesystem::path, FileRecord> m_Files;
    std::vector<std::string> m_AllowedFileTypes;
};
