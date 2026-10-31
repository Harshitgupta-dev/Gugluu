#pragma once

#include <cstddef>
#include <cstdint>

namespace GugluuConstants
{
	inline constexpr std::size_t c_MaxSequenceLength = 256;
	inline constexpr std::size_t c_EmbeddingDimensions = 384;
	inline constexpr std::size_t c_SpecialTokenCount = 2;
	inline constexpr std::size_t c_MaxTokenCharacters = 100;
	inline constexpr std::size_t c_TopResultCount = 10;
	inline constexpr std::uint32_t c_DataFormatVersion = 1;

	inline constexpr char c_AllowedFileTypesFileName[] = "AllowedFileTypes.txt";
	inline constexpr char c_DataFileName[] = "Data.gdata";
	inline constexpr char c_ModelFilePath[] =
		"Vendor/Model/all-MiniLM-L6-v2/model.onnx";
	inline constexpr char c_VocabularyFilePath[] =
		"Vendor/Model/all-MiniLM-L6-v2/vocab.txt";
}
