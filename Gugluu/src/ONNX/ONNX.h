#pragma once

#include "Gugluu/src/pch.h"

class ONNX
{
public:
	static std::vector<float> RunModel(const TokenizedText& a_TokenizedText);

private:
	ONNX();
	std::vector<float> RunModelImpl(const TokenizedText& a_TokenizedText);

	Ort::Env m_Environment;
	Ort::SessionOptions m_SessionOptions;
	Ort::Session m_Session;
	Ort::MemoryInfo m_MemoryInfo;
};
