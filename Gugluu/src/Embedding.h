#pragma once

#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "Gugluu/src/Constants.h"
#include "Gugluu/src/Macros.h"
#include "Gugluu/src/Tokenizer/Tokenizer.h"
#include "Gugluu/src/ONNX/ONNX.h"

inline std::vector<std::vector<float>> CreateEmbeddings(
	const std::string& a_InputString)
{
	const TokenizedText c_Tokenized = Tokenizer::Tokenize(a_InputString);
	const std::vector<float> c_Tensor = ONNX::RunModel(c_Tokenized);
	const std::size_t c_SequenceLength = GugluuConstants::c_MaxSequenceLength;
	const std::size_t c_Dimensions = GugluuConstants::c_EmbeddingDimensions;

	GUGLUU_ASSERT(
		!c_Tokenized.m_InputIds.empty()
			&& c_Tokenized.m_InputIds.size() == c_Tokenized.m_AttentionMask.size()
			&& c_Tokenized.m_InputIds.size() % c_SequenceLength == 0,
		"Tokenized input has an invalid shape.");
	GUGLUU_ASSERT(
		c_Tensor.size() == c_Tokenized.m_InputIds.size() * c_Dimensions,
		"The model output has an unexpected shape.");

	const std::size_t c_ChunkCount =
		c_Tokenized.m_InputIds.size() / c_SequenceLength;
	std::vector<std::vector<float>> embeddings;
	embeddings.reserve(c_ChunkCount);

	for (std::size_t chunkIndex = 0; chunkIndex < c_ChunkCount; ++chunkIndex)
	{
		std::vector<float> pooledEmbedding(c_Dimensions, 0.0f);
		std::size_t validTokenCount = 0;

		for (std::size_t tokenIndex = 0;
			tokenIndex < c_SequenceLength;
			++tokenIndex)
		{
			const std::size_t c_MaskIndex =
				chunkIndex * c_SequenceLength + tokenIndex;
			if (c_Tokenized.m_AttentionMask[c_MaskIndex] == 0)
				continue;

			++validTokenCount;
			const std::size_t c_TokenOffset = c_MaskIndex * c_Dimensions;
			for (std::size_t dimension = 0; dimension < c_Dimensions; ++dimension)
				pooledEmbedding[dimension] += c_Tensor[c_TokenOffset + dimension];
		}

		GUGLUU_ASSERT(validTokenCount > 0, "A token chunk contains no valid tokens.");
		for (float& value : pooledEmbedding)
			value /= static_cast<float>(validTokenCount);

		float magnitudeSquared = 0.0f;
		for (float value : pooledEmbedding)
			magnitudeSquared += value * value;

		const float c_Magnitude = std::sqrt(magnitudeSquared);
		GUGLUU_ASSERT(std::isfinite(c_Magnitude), "Model returned a non-finite embedding.");
		if (c_Magnitude > 0.0f)
		{
			for (float& value : pooledEmbedding)
				value /= c_Magnitude;
		}

		embeddings.emplace_back(std::move(pooledEmbedding));
	}

	return embeddings;
}

inline std::vector<float> CreateEmbedding(const std::string& a_InputString)
{
	const std::vector<std::vector<float>> c_Embeddings =
		CreateEmbeddings(a_InputString);
	GUGLUU_ASSERT(
		!c_Embeddings.empty(),
		"The input did not produce any embeddings.");

	const std::size_t c_Dimensions = GugluuConstants::c_EmbeddingDimensions;
	std::vector<float> meanEmbedding(c_Dimensions, 0.0f);
	for (const auto& c_Embedding : c_Embeddings)
	{
		GUGLUU_ASSERT(
			c_Embedding.size() == c_Dimensions,
			"An embedding has an unexpected dimension.");
		for (std::size_t dimension = 0; dimension < c_Dimensions; ++dimension)
			meanEmbedding[dimension] += c_Embedding[dimension];
	}

	for (float& value : meanEmbedding)
		value /= static_cast<float>(c_Embeddings.size());

	float magnitudeSquared = 0.0f;
	for (float value : meanEmbedding)
		magnitudeSquared += value * value;
	const float c_Magnitude = std::sqrt(magnitudeSquared);
	GUGLUU_ASSERT(std::isfinite(c_Magnitude), "Query embedding magnitude is not finite.");
	if (c_Magnitude > 0.0f)
	{
		for (float& value : meanEmbedding)
			value /= c_Magnitude;
	}

	return meanEmbedding;
}
