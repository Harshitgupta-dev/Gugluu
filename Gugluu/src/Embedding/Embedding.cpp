#include "Gugluu/src/core/pch.h"
#include "Embedding.h"


std::vector<float> CreateEmbeddings(
	const TokenizedText& a_Tokenized)
{
	const std::size_t c_SequenceLength =
		GugluuConstants::c_MaxSequenceLength;
	const std::size_t c_Dimensions =
		GugluuConstants::c_EmbeddingDimensions;

	GUGLUU_ASSERT(
		!a_Tokenized.InputIds.empty()
		&& a_Tokenized.InputIds.size() == a_Tokenized.AttentionMask.size()
		&& a_Tokenized.InputIds.size() % c_SequenceLength == 0,
		"Tokenized input has an invalid shape.");

	const std::vector<float> c_Tensor =
		ONNX::s_RunModel(a_Tokenized);

	const std::size_t c_TokenCount = a_Tokenized.InputIds.size();
	GUGLUU_ASSERT(
		c_TokenCount <= std::numeric_limits<std::size_t>::max() / c_Dimensions,
		"Embedding tensor size exceeds the addressable range.");

	const std::size_t c_ExpectedTensorSize = c_TokenCount * c_Dimensions;
	const std::size_t c_ChunkCount = c_TokenCount / c_SequenceLength;

	GUGLUU_ASSERT(
		c_Tensor.size() == c_ExpectedTensorSize,
		"ONNX output does not match the tokenized input shape.");

	std::vector<float> embeddings(c_ChunkCount * c_Dimensions);

	for (std::size_t chunk = 0; chunk < c_ChunkCount; ++chunk)
	{
		float* const c_Embedding =
			embeddings.data() + chunk * c_Dimensions;

		float c_TokenCount = 0.0f;

		for (std::size_t token = 0; token < c_SequenceLength; ++token)
		{
			const std::size_t c_MaskIndex =
				chunk * c_SequenceLength + token;

			if (a_Tokenized.AttentionMask[c_MaskIndex] == 0)
				continue;

			const std::size_t c_TensorIndex =
				(chunk * c_SequenceLength + token) * c_Dimensions;

			for (std::size_t dimension = 0;
				dimension < c_Dimensions;
				++dimension)
			{
				c_Embedding[dimension] +=
					c_Tensor[c_TensorIndex + dimension];
			}

			c_TokenCount += 1.0f;
		}

		// Mean pooling
		GUGLUU_ASSERT(
			c_TokenCount > 0.0f,
			"Chunk contains no attended tokens.");

		for (std::size_t dimension = 0;
			dimension < c_Dimensions;
			++dimension)
		{
			c_Embedding[dimension] /= c_TokenCount;
		}

		// L2 normalization
		float c_NormSquared = 0.0f;

		for (std::size_t dimension = 0;
			dimension < c_Dimensions;
			++dimension)
		{
			c_NormSquared +=
				c_Embedding[dimension] *
				c_Embedding[dimension];
		}

		const float c_Norm = std::sqrt(c_NormSquared);

		GUGLUU_ASSERT(
			std::isfinite(c_Norm) && c_Norm > 0.0f,
			"Cannot normalize a zero-length embedding.");

		for (std::size_t dimension = 0;
			dimension < c_Dimensions;
			++dimension)
		{
			c_Embedding[dimension] /= c_Norm;
		}
	}

	return embeddings;
}