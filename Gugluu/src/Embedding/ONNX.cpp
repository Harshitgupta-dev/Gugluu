#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/Embedding/ONNX.h"

ONNX::ONNX() :
	m_Environment(ORT_LOGGING_LEVEL_WARNING, "Gugluu"),
	m_SessionOptions(),
	m_Session(
		m_Environment,
		UtilsGugluu::ResourcePath(
			GugluuConstants::c_ModelFilePath).c_str(),
		m_SessionOptions),
	m_MemoryInfo("Cpu", OrtAllocatorType::OrtArenaAllocator, 0, OrtMemTypeDefault)
{
}

std::vector<float> ONNX::RunModelImpl(
	const TokenizedText& a_TokenizedText)
{
	const std::size_t c_SequenceLength =
		GugluuConstants::c_MaxSequenceLength;

	const std::size_t c_MaxBatchSize =
		GugluuConstants::c_MaxBatchSize;

	const std::size_t c_ChunkCount =
		a_TokenizedText.InputIds.size() / c_SequenceLength;


	const std::size_t c_Dimensions =
		GugluuConstants::c_EmbeddingDimensions;

	std::vector<float> output;
	output.reserve(
		c_ChunkCount * c_Dimensions);

	std::vector<std::int64_t> tokenTypeIds(
		c_MaxBatchSize * c_SequenceLength,
		0);

	const char* c_InputNames[] =
	{
		"input_ids",
		"attention_mask",
		"token_type_ids"
	};

	const char* c_OutputNames[] =
	{
		"last_hidden_state"
	};

	for (std::size_t c_StartChunk = 0;
		c_StartChunk < c_ChunkCount;
		c_StartChunk += c_MaxBatchSize)
	{
		const std::size_t c_BatchSize =
			std::min(
				c_MaxBatchSize,
				c_ChunkCount - c_StartChunk);

		const std::size_t c_BatchValues =
			c_BatchSize * c_SequenceLength;

		const std::size_t c_InputOffset =
			c_StartChunk * c_SequenceLength;

		const std::vector<std::int64_t> c_Shape =
		{
			static_cast<std::int64_t>(c_BatchSize),
			static_cast<std::int64_t>(c_SequenceLength)
		};

		Ort::Value inputIdsTensor =
			Ort::Value::CreateTensor<int64_t>(
				m_MemoryInfo,
				const_cast<std::int64_t*>(
					a_TokenizedText.InputIds.data()
					+ c_InputOffset),
				c_BatchValues,
				c_Shape.data(),
				c_Shape.size()
			);

		Ort::Value attentionMaskTensor =
			Ort::Value::CreateTensor<int64_t>(
				m_MemoryInfo,
				const_cast<std::int64_t*>(
					a_TokenizedText.AttentionMask.data()
					+ c_InputOffset),
				c_BatchValues,
				c_Shape.data(),
				c_Shape.size()
			);

		Ort::Value tokenTypeIdsTensor =
			Ort::Value::CreateTensor<int64_t>(
				m_MemoryInfo,
				tokenTypeIds.data(),
				c_BatchValues,
				c_Shape.data(),
				c_Shape.size()
			);

		Ort::Value inputTensors[] =
		{
			std::move(inputIdsTensor),
			std::move(attentionMaskTensor),
			std::move(tokenTypeIdsTensor)
		};

		auto result = m_Session.Run(
			Ort::RunOptions{ nullptr },
			c_InputNames,
			inputTensors,
			3,
			c_OutputNames,
			1
		);

		GUGLUU_ASSERT(
			!result.empty(),
			"The ONNX model returned no outputs.");

		const std::vector<std::int64_t> c_OutputShape =
			result[0].GetTensorTypeAndShapeInfo().GetShape();

		GUGLUU_ASSERT(
			c_OutputShape.size() == 3
			&& c_OutputShape[0]
			== static_cast<std::int64_t>(c_BatchSize)
			&& c_OutputShape[1]
			== static_cast<std::int64_t>(c_SequenceLength)
			&& c_OutputShape[2]
			== static_cast<std::int64_t>(
				GugluuConstants::c_EmbeddingDimensions),
			"The ONNX model returned an unexpected output shape.");

		const std::size_t c_OutputValues =
			c_BatchValues
			* GugluuConstants::c_EmbeddingDimensions;

		const float* c_OutputData =
			result[0].GetTensorData<float>();

		output.insert(
			output.end(),
			c_OutputData,
			c_OutputData + c_OutputValues);
	}

	GUGLUU_ASSERT(
		output.size()
		== a_TokenizedText.InputIds.size() * c_Dimensions,
		"The model output has an unexpected shape.");

	return output;
}

std::vector<float> ONNX::s_RunModel(const TokenizedText& a_TokenizedText)
{
	static ONNX s_Model;
	return s_Model.RunModelImpl(a_TokenizedText);
}
