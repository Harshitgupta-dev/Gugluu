#pragma once

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <numeric>
#include <stdexcept>
#include <unordered_map>
#include <utility>
#include <vector>

#include "Gugluu/src/FileRecord.h"

namespace Gugluu
{
	using SimilarityMatch = std::pair<float, std::filesystem::path>;

	inline std::vector<SimilarityMatch> CalculateSimilarity(
		const std::unordered_map<std::filesystem::path, FileRecord>& a_Records,
		const std::vector<float>& a_QueryEmbedding)
	{
		if (a_QueryEmbedding.empty())
			throw std::invalid_argument("The query embedding is empty.");
		if (!std::all_of(
			a_QueryEmbedding.begin(),
			a_QueryEmbedding.end(),
			[](float a_Value) { return std::isfinite(a_Value); }))
			throw std::invalid_argument("The query embedding contains a non-finite value.");

		std::vector<SimilarityMatch> matches;
		matches.reserve(a_Records.size());

		for (const auto& [path, record] : a_Records)
		{
			if (record.GetEmbedding().size() != a_QueryEmbedding.size())
				throw std::runtime_error(
					"Embedding dimensions do not match for file: " + path.string());

			if (!std::all_of(
				record.GetEmbedding().begin(),
				record.GetEmbedding().end(),
				[](float a_Value) { return std::isfinite(a_Value); }))
				throw std::runtime_error(
					"Embedding contains a non-finite value for file: "
					+ path.string());

			const float c_Score = std::inner_product(
				a_QueryEmbedding.begin(),
				a_QueryEmbedding.end(),
				record.GetEmbedding().begin(),
				0.0f);
			if (!std::isfinite(c_Score))
				throw std::runtime_error(
					"Similarity score is not finite for file: " + path.string());
			matches.emplace_back(c_Score, path);
		}

		std::sort(matches.begin(), matches.end(),
			[](const SimilarityMatch& a_Left, const SimilarityMatch& a_Right)
			{
				if (a_Left.first != a_Right.first)
					return a_Left.first > a_Right.first;
				return a_Left.second < a_Right.second;
			});

		return matches;
	}
}
