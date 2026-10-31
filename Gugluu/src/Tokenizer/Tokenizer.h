#pragma once

#include "Gugluu/src/pch.h"

using VocabMap = std::unordered_map<std::string, std::int64_t>;

inline std::ostream& operator<<(std::ostream& a_Os, const TokenizedText& a_Text)
{
	a_Os << "input IDs: " << a_Text.m_InputIds
		<< " attention mask: " << a_Text.m_AttentionMask;
	return a_Os;
}

class Tokenizer
{
public:
	static TokenizedText Tokenize(const std::string& a_InputString);

private:
	explicit Tokenizer(const std::filesystem::path& a_FilePath);

	TokenizedText TokenizeText(const std::string& a_InputString);
	void LoadVocab(const std::filesystem::path& a_FilePath);
	std::string Normalize(const std::string& a_InputString);
	std::vector<std::string> PreTokenize(const std::string& a_InputString);
	std::vector<std::string> WordPieceTokenize(const std::string& a_Word);
	std::vector<std::int64_t> ConvertToIds(
		const std::vector<std::string>& a_Tokens);
	TokenizedText Chunker(const std::vector<std::int64_t>& a_TokenIds);

	VocabMap m_Vocabulary;
};
