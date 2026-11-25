#pragma once

#include "Gugluu/src/core/pch.h"

struct VocabHash
{
	using is_transparent = void;

	size_t operator()(
		std::string_view a_String) const noexcept
	{
		return std::hash<std::string_view>{}(a_String);
	}
};

struct VocabEqual
{
	using is_transparent = void;

	bool operator()(
		std::string_view a_Left,
		std::string_view a_Right) const noexcept
	{
		return a_Left == a_Right;
	}
};

using VocabMap =
std::unordered_map<
	std::string,
	std::int64_t,
	VocabHash,
	VocabEqual>;


class Tokenizer
{
public:

	static TokenizedText s_Tokenize(
		const std::string& a_InputString);


private:

	explicit Tokenizer(
		const std::filesystem::path& a_FilePath);


	void LoadVocab(
		const std::filesystem::path& a_FilePath);


	TokenizedText TokenizeText(
		std::string& a_InputString);


	void Normalize(
		std::string& a_InputString);


	std::vector<std::string> PreTokenize(
		const std::string& a_InputString);


	void WordPieceTokenize(
		const std::string& a_Word,
		std::vector<std::string>& a_Output);


	void ChunkAndConvert(
		const std::vector<std::string>& a_Tokens,
		TokenizedText& a_Result);


	std::int64_t GetTokenId(
		std::string_view a_Token) const;


	VocabMap m_Vocabulary;

	std::int64_t m_CLSId = 0;
	std::int64_t m_SEPId = 0;
	std::int64_t m_PADId = 0;
};