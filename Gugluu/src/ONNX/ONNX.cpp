#include "Gugluu/src/pch.h"
#include "Gugluu/src/ONNX/ONNX.h"
#include "Gugluu/src/utils.h"

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

std::vector<float> ONNX::RunModelImpl(const TokenizedText& a_TokenizedText)
{
	GUGLUU_ASSERT(
		!a_TokenizedText.m_InputIds.empty()
			&& a_TokenizedText.m_InputIds.size()
				== a_TokenizedText.m_AttentionMask.size()
			&& a_TokenizedText.m_InputIds.size()
				% GugluuConstants::c_MaxSequenceLength == 0,
		"Tokenized input has an invalid shape.");

	const std::vector<std::int64_t> c_Shape =
	{
		static_cast<std::int64_t>(
			a_TokenizedText.m_InputIds.size()
				/ GugluuConstants::c_MaxSequenceLength),
		static_cast<std::int64_t>(GugluuConstants::c_MaxSequenceLength)
	};
	std::vector<std::int64_t> tokenTypeIds(a_TokenizedText.m_InputIds.size(), 0);

	Ort::Value inputIdsTensor =
		Ort::Value::CreateTensor<int64_t>(
			m_MemoryInfo,
			const_cast<std::int64_t*>(a_TokenizedText.m_InputIds.data()),
			a_TokenizedText.m_InputIds.size(),
			c_Shape.data(),
			c_Shape.size()
		);

	Ort::Value attentionMaskTensor =
		Ort::Value::CreateTensor<int64_t>(
			m_MemoryInfo,
			const_cast<std::int64_t*>(a_TokenizedText.m_AttentionMask.data()),
			a_TokenizedText.m_AttentionMask.size(),
			c_Shape.data(),
			c_Shape.size()
		);

	Ort::Value tokenTypeIdsTensor =
		Ort::Value::CreateTensor<int64_t>(
			m_MemoryInfo,
			tokenTypeIds.data(),
			tokenTypeIds.size(),
			c_Shape.data(),
			c_Shape.size()
		);

	const char* c_InputNames[] =
	{
		"input_ids",
		"attention_mask",
		"token_type_ids"
	};

	Ort::Value inputTensors[] =
	{
		std::move(inputIdsTensor),
		std::move(attentionMaskTensor),
		std::move(tokenTypeIdsTensor)
	};

	const char* c_OutputNames[] =
	{
		"last_hidden_state"
	};

	auto result = m_Session.Run(
		Ort::RunOptions{ nullptr },
		c_InputNames,
		inputTensors,
		3,
		c_OutputNames,
		1
	);

	GUGLUU_ASSERT(!result.empty(), "The ONNX model returned no outputs.");

	const std::vector<std::int64_t> c_OutputShape = result[0]
		.GetTensorTypeAndShapeInfo()
		.GetShape();
	GUGLUU_ASSERT(
		c_OutputShape.size() == 3
			&& c_OutputShape[0] == c_Shape[0]
			&& c_OutputShape[1]
				== static_cast<std::int64_t>(GugluuConstants::c_MaxSequenceLength)
			&& c_OutputShape[2]
				== static_cast<std::int64_t>(GugluuConstants::c_EmbeddingDimensions),
		"The ONNX model returned an unexpected output shape.");

	const std::size_t c_TotalValues =
		a_TokenizedText.m_InputIds.size()
		* GugluuConstants::c_EmbeddingDimensions;
	const float* c_OutputData = result[0].GetTensorData<float>();
	return std::vector<float>(c_OutputData, c_OutputData + c_TotalValues);
}

std::vector<float> ONNX::RunModel(const TokenizedText& a_TokenizedText)
{
	static ONNX s_Model;
	return s_Model.RunModelImpl(a_TokenizedText);
}
