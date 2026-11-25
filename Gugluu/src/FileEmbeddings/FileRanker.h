#pragma once

#include "Gugluu/src/FileEmbeddings/DataLoader.h"
#include "Gugluu/src/SimilarityModel/FAISS.h"

class FileRanker final
{
public:
	struct RankedFile
	{
		const FileRecord* record;
		double similarityRank;
	};

	static std::vector<RankedFile> s_Rank(
		const DataLoader::Data& a_FileData,
		const std::vector<SimilarityResult>& a_ChunkMatches)
	{
		struct ChunkScores
		{
			float best = (std::numeric_limits<float>::lowest)();
			float secondBest = (std::numeric_limits<float>::lowest)();
			std::size_t matchedChunkCount = 0;
			double sum = 0.0;
		};

		std::vector<ChunkScores> scores(a_FileData.files.size());
		std::unordered_map<std::uint64_t, std::size_t> fileIndexById;
		fileIndexById.reserve(a_FileData.files.size());

		for (std::size_t i = 0; i < a_FileData.files.size(); ++i)
			fileIndexById.emplace(a_FileData.files[i].GetId(), i);

		for (const SimilarityResult& match : a_ChunkMatches)
		{
			if (match.ChunkIndex >= a_FileData.chunkEmbeddings.owner.size())
				throw std::logic_error("Matched chunk has no owner ID.");

			const std::uint64_t ownerId =
				a_FileData.chunkEmbeddings.owner[match.ChunkIndex];
			const auto fileIndex = fileIndexById.find(ownerId);

			if (fileIndex == fileIndexById.end())
				throw std::logic_error("Matched chunk owner has no file record.");

			ChunkScores& fileScores = scores[fileIndex->second];
			++fileScores.matchedChunkCount;
			fileScores.sum += match.Similarity;

			if (match.Similarity > fileScores.best)
			{
				fileScores.secondBest = fileScores.best;
				fileScores.best = match.Similarity;
			}
			else if (match.Similarity > fileScores.secondBest)
			{
				fileScores.secondBest = match.Similarity;
			}
		}

		std::vector<RankedFile> rankedFiles;
		rankedFiles.reserve(a_FileData.files.size());

		for (std::size_t i = 0; i < a_FileData.files.size(); ++i)
		{
			const FileRecord& record = a_FileData.files[i];
			const bool hasUnmatchedChunks =
				scores[i].matchedChunkCount < record.GetChunkCount();
			const double bestMatchedSimilarity =
				scores[i].matchedChunkCount == 0 ? 0.0 : scores[i].best;
			const double secondMatchedSimilarity =
				scores[i].matchedChunkCount < 2 ? 0.0 : scores[i].secondBest;
			const double bestSimilarity = hasUnmatchedChunks
				? (std::max)(0.0, bestMatchedSimilarity)
				: bestMatchedSimilarity;
			const double secondBestSimilarity = hasUnmatchedChunks
				? (std::max)(0.0, secondMatchedSimilarity)
				: secondMatchedSimilarity;
			const double meanSimilarity = record.GetChunkCount() == 0
				? 0.0
				: scores[i].sum / record.GetChunkCount();
			const double rank =
				0.5 * bestSimilarity
				+ 0.3 * secondBestSimilarity
				+ 0.2 * meanSimilarity;

			rankedFiles.push_back({ &record, rank });
		}

		std::stable_sort(
			rankedFiles.begin(),
			rankedFiles.end(),
			[](const RankedFile& a_Left, const RankedFile& a_Right)
			{
				return a_Left.similarityRank > a_Right.similarityRank;
			});

		return rankedFiles;
	}
};
