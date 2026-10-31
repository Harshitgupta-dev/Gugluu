#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class FileRecord
{
public:
	FileRecord(std::filesystem::path a_FilePath);
	FileRecord(
		std::uint32_t a_Id,
		std::filesystem::path a_FilePath,
		std::uintmax_t a_FileSize,
		std::filesystem::file_time_type a_LastModified,
		std::vector<std::vector<float>> a_Embeddings);

	bool HasChanged() const;
	void OnUpdate();

	std::uint32_t GetId() const noexcept;
	const std::filesystem::path& GetPath() const noexcept;
	std::uintmax_t GetFileSize() const noexcept;
	std::filesystem::file_time_type GetLastModified() const noexcept;
	const std::vector<std::vector<float>>& GetEmbeddings() const noexcept;
	const std::vector<float>& GetEmbedding() const noexcept;

	static std::string ReadFile(const std::filesystem::path& a_FilePath);

private:
	static std::uint32_t s_IdCount;

	std::uint32_t m_Id;
	std::filesystem::path m_Path;
	std::uintmax_t m_FileSize;
	std::filesystem::file_time_type m_LastModified;
	std::vector<std::vector<float>> m_Embeddings;
	std::vector<float> m_Embedding;
};

