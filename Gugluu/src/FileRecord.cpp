#include "Gugluu/src/pch.h"
#include "Gugluu/src/FileRecord.h"
#include "Gugluu/src/Embedding.h"

#include <algorithm>
#include <limits>
#include <stdexcept>
#include <utility>

namespace
{
	std::vector<float> AverageEmbeddings(
		const std::vector<std::vector<float>>& a_Embeddings)
	{
		if (a_Embeddings.empty())
			return {};

		const std::size_t c_Dimensions = a_Embeddings.front().size();
		GUGLUU_ASSERT(c_Dimensions != 0, "Embedding dimensions cannot be zero.");

		std::vector<float> average(c_Dimensions, 0.0f);
		for (const auto& embedding : a_Embeddings)
		{
			GUGLUU_ASSERT(
				embedding.size() == c_Dimensions,
				"A file record contains embeddings with inconsistent dimensions.");

			for (std::size_t index = 0; index < c_Dimensions; ++index)
				average[index] += embedding[index];
		}

		for (float& value : average)
			value /= static_cast<float>(a_Embeddings.size());

		float magnitudeSquared = 0.0f;
		for (float value : average)
		{
			GUGLUU_ASSERT(std::isfinite(value), "A file embedding contains a non-finite value.");
			magnitudeSquared += value * value;
		}
		const float c_Magnitude = std::sqrt(magnitudeSquared);
		GUGLUU_ASSERT(std::isfinite(c_Magnitude), "A file embedding magnitude is not finite.");
		if (c_Magnitude > 0.0f)
		{
			for (float& value : average)
				value /= c_Magnitude;
		}

		return average;
	}
}

std::uint32_t FileRecord::s_IdCount = 0;

FileRecord::FileRecord(std::filesystem::path a_FilePath) :
	m_Id(s_IdCount),
	m_Path(std::move(a_FilePath)),
	m_FileSize(std::filesystem::file_size(m_Path)),
	m_LastModified(std::filesystem::last_write_time(m_Path)),
	m_Embeddings(CreateEmbeddings(ReadFile(m_Path))),
	m_Embedding(AverageEmbeddings(m_Embeddings))
{
	GUGLUU_ASSERT(
		s_IdCount != (std::numeric_limits<std::uint32_t>::max)(),
		"No additional file record IDs are available.");
	++s_IdCount;
}

FileRecord::FileRecord(
	std::uint32_t a_Id,
	std::filesystem::path a_FilePath,
	std::uintmax_t a_FileSize,
	std::filesystem::file_time_type a_LastModified,
	std::vector<std::vector<float>> a_Embeddings) :
	m_Id(a_Id),
	m_Path(std::move(a_FilePath)),
	m_FileSize(a_FileSize),
	m_LastModified(a_LastModified),
	m_Embeddings(std::move(a_Embeddings)),
	m_Embedding(AverageEmbeddings(m_Embeddings))
{
	GUGLUU_ASSERT(
		m_Id != (std::numeric_limits<std::uint32_t>::max)(),
		"File record ID is too large to load.");
	s_IdCount = (std::max)(s_IdCount, m_Id + 1);
}

bool FileRecord::HasChanged() const
{
	return std::filesystem::file_size(m_Path) != m_FileSize
		|| std::filesystem::last_write_time(m_Path) != m_LastModified;
}

void FileRecord::OnUpdate()
{
	if (!HasChanged())
		return;

	auto updatedEmbeddings = CreateEmbeddings(ReadFile(m_Path));
	auto updatedAverage = AverageEmbeddings(updatedEmbeddings);
	const auto c_UpdatedFileSize = std::filesystem::file_size(m_Path);
	const auto c_UpdatedLastModified = std::filesystem::last_write_time(m_Path);

	m_Embeddings = std::move(updatedEmbeddings);
	m_Embedding = std::move(updatedAverage);
	m_FileSize = c_UpdatedFileSize;
	m_LastModified = c_UpdatedLastModified;
}

std::uint32_t FileRecord::GetId() const noexcept
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

const std::vector<std::vector<float>>& FileRecord::GetEmbeddings() const noexcept
{
    return m_Embeddings;
}

const std::vector<float>& FileRecord::GetEmbedding() const noexcept
{
    return m_Embedding;
}

std::string FileRecord::ReadFile(const std::filesystem::path& a_FilePath)
{
	std::ifstream file(a_FilePath);
	if (!file.is_open())
	{
		throw std::filesystem::filesystem_error(
			"Unable to open file for reading",
			a_FilePath,
			std::make_error_code(std::errc::io_error));
	}

	const std::istreambuf_iterator<char> c_Begin(file);
	const std::istreambuf_iterator<char> c_End;
	std::string content(c_Begin, c_End);
	if (file.bad())
	{
		throw std::filesystem::filesystem_error(
			"Unable to finish reading file",
			a_FilePath,
			std::make_error_code(std::errc::io_error));
	}

	return content;
}

