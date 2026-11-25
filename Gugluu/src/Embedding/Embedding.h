#pragma once
#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/Embedding/ONNX.h"

std::vector<float> CreateEmbeddings(
	const TokenizedText& a_Tokenized);