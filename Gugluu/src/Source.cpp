#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/Embedding/Embedding.h"
#include "Gugluu/src/FileEmbeddings/DataLoader.h"
#include "Gugluu/src/SimilarityModel/FAISS.h"
#include "Gugluu/src/Tokenizer/Tokenizer.h"
#include "Gugluu/src/FileEmbeddings/FileRanker.h"

namespace
{
	constexpr std::size_t c_TopMatchedChunks = 500;
	constexpr std::size_t c_TopFilesToPrint = 10;
}

int main()
{
	Log::s_Init();
	try
	{
		DataLoader::s_Bind(
			[](const std::vector<float>& a_Embeddings)
			{
				FAISS::s_Initialize(a_Embeddings);
			},

			[](const TokenizedText& a_Tokenized)
			{
				return CreateEmbeddings(a_Tokenized);
			},

			[](const std::string& a_InputString)
			{
				return Tokenizer::s_Tokenize(a_InputString);
			}
		);

		std::cout << "Directory to index: ";
		std::string directory;
		if (!std::getline(std::cin, directory) || directory.empty())
		{
			LOG_ERROR("A directory path is required.");
			return 1;
		}

		std::error_code directoryError;
		if (!std::filesystem::is_directory(directory, directoryError))
		{
			LOG_ERROR(
				"The indexing path is not a readable directory: {}{}",
				directory,
				directoryError ? " (" + directoryError.message() + ")" : "");
			return 1;
		}

		const DataLoader::Data fileData =
			DataLoader::s_GetData(std::filesystem::path(directory));

		std::string query;
		while (true)
		{
			std::cout << "Search query (leave blank to quit): ";
			std::getline(std::cin, query);
			if (query.empty())
				break;

			const TokenizedText tokenizedQuery = Tokenizer::s_Tokenize(query);
			if (tokenizedQuery.InputIds.empty())
			{
				LOG_WARN("The query contains no searchable text.");
				continue;
			}

			const std::vector<float> queryEmbedding =
				CreateEmbeddings(tokenizedQuery);
			const auto chunkMatches = FAISS::s_GetSimilarityMap(
				queryEmbedding,
				c_TopMatchedChunks
			);
			const std::vector<FileRanker::RankedFile> rankedFiles =
				FileRanker::s_Rank(fileData, chunkMatches);

			std::cout << "Top matching files:\n";
			const std::size_t filesToPrint =
				(std::min)(c_TopFilesToPrint, rankedFiles.size());

			for (std::size_t i = 0; i < filesToPrint; ++i)
			{
				std::cout << i + 1 << ". "
					<< rankedFiles[i].record->GetPath().string()
					<< " (similarity rank: "
					<< std::fixed << std::setprecision(4)
					<< rankedFiles[i].similarityRank << ")\n";
			}
		}
	}
	catch (const std::exception& exception)
	{
		LOG_ERROR("Gugluu failed: {}", exception.what());
		return 1;
	}

	return 0;
}
