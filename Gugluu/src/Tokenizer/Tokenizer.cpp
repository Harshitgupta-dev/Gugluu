#include "Gugluu/src/pch.h"
#include "Gugluu/src/Tokenizer/Tokenizer.h"
#include "Gugluu/src/utils.h"

Tokenizer::Tokenizer(const std::filesystem::path& a_FilePath)
{
	LoadVocab(a_FilePath);
}

void Tokenizer::LoadVocab(const std::filesystem::path& a_FilePath)
{
	std::ifstream vocab(a_FilePath);

	GUGLUU_ASSERT(vocab.is_open(), "Failed to load vocabulary file: " << a_FilePath.string());

	std::string line;
	int64_t index = 0;

	while (std::getline(vocab, line))
		m_Vocabulary[line] = index++;

	GUGLUU_ASSERT(!m_Vocabulary.empty(), "The vocabulary file is empty.");
}

TokenizedText Tokenizer::TokenizeText(
	const std::string& a_InputString)
{
	// Normalize
	const std::string c_Normalized = Normalize(a_InputString);

	// BERT pre-tokenization
	std::vector<std::string> preTokenized =
		PreTokenize(c_Normalized);

	// WordPiece tokenization
	std::vector<std::string> wordPieceTokenized;

	for (const auto& word : preTokenized)
	{
		std::vector<std::string> tokens =
			WordPieceTokenize(word);

		wordPieceTokenized.insert(
			wordPieceTokenized.end(),
			tokens.begin(),
			tokens.end()
		);
	}

	// Convert vocabulary tokens into IDs
	const std::vector<std::int64_t> c_Tokenized =
		ConvertToIds(wordPieceTokenized);

	// Chunk, add special tokens and pad
	auto chunks = Chunker(c_Tokenized);
	return chunks;
}

std::string Tokenizer::Normalize(
	const std::string& a_InputString)
{
	std::string normalized = UtilsGugluu::ToLowerCase(a_InputString);
	UtilsGugluu::NormalizeWhitespace(normalized);
	return normalized;
}

std::vector<std::string> Tokenizer::PreTokenize(
	const std::string& a_InputString)
{
	std::vector<std::string> stringVector;
	std::string currentToken;

	for (unsigned char c : a_InputString)
	{
		if (std::isspace(c))
		{
			if (!currentToken.empty())
			{
				stringVector.emplace_back(currentToken);
				currentToken.clear();
			}
		}
		else if (std::ispunct(c))
		{
			if (!currentToken.empty())
			{
				stringVector.emplace_back(currentToken);
				currentToken.clear();
			}

			stringVector.emplace_back(std::string(1, c));
		}
		else
		{
			currentToken.push_back(c);
		}
	}

	if (!currentToken.empty())
	{
		stringVector.emplace_back(currentToken);
	}

	return stringVector;
}

std::vector<std::string> Tokenizer::WordPieceTokenize(
	const std::string& a_Word)
{
	std::vector<std::string> tokens;

	if (a_Word.empty())
		return tokens;

	// BERT WordPiece maximum character limit
	if (a_Word.length() > GugluuConstants::c_MaxTokenCharacters)
	{
		tokens.emplace_back("[UNK]");
		return tokens;
	}

	// Whole word exists in vocabulary
	if (m_Vocabulary.contains(a_Word))
	{
		tokens.emplace_back(a_Word);
		return tokens;
	}

	size_t start = 0;

	while (start < a_Word.length())
	{
		size_t end = a_Word.length();

		std::string currentToken;
		bool found = false;

		// Greedy longest-match-first
		while (start < end)
		{
			std::string piece =
				a_Word.substr(start, end - start);

			// Continuation pieces require ##
			if (start != 0)
				piece = "##" + piece;

			if (m_Vocabulary.contains(piece))
			{
				currentToken = piece;
				found = true;
				break;
			}

			--end;
		}

		// Word cannot be represented by vocabulary
		if (!found)
		{
			return { "[UNK]" };
		}

		tokens.emplace_back(currentToken);

		// Move forward by the number of actual characters consumed
		if (currentToken.starts_with("##"))
			start += currentToken.length() - 2;
		else
			start += currentToken.length();
	}

	return tokens;
}

std::vector<int64_t> Tokenizer::ConvertToIds(
	const std::vector<std::string>& a_Tokens)
{
	std::vector<int64_t> tokenizedVector;

	tokenizedVector.reserve(a_Tokens.size());

	for (const auto& tokenString : a_Tokens)
	{
		auto iterator = m_Vocabulary.find(tokenString);

		GUGLUU_ASSERT(
			iterator != m_Vocabulary.end(),
			"Token does not exist in vocabulary: " << tokenString);

		tokenizedVector.emplace_back(iterator->second);
	}

	return tokenizedVector;
}

TokenizedText Tokenizer::Chunker(
	const std::vector<std::int64_t>& a_TokenIds)
{
	const std::size_t c_MaxContentLength =
		GugluuConstants::c_MaxSequenceLength
		- GugluuConstants::c_SpecialTokenCount;

	GUGLUU_ASSERT(
		m_Vocabulary.contains("[CLS]")
			&& m_Vocabulary.contains("[SEP]")
			&& m_Vocabulary.contains("[PAD]"),
		"The vocabulary is missing a required special token.");

	const std::int64_t c_ClsId = m_Vocabulary.at("[CLS]");
	const std::int64_t c_SepId = m_Vocabulary.at("[SEP]");
	const std::int64_t c_PadId = m_Vocabulary.at("[PAD]");
	TokenizedText chunks;

	for (std::size_t offset = 0;
		offset == 0 || offset < a_TokenIds.size();
		offset += c_MaxContentLength)
	{
		const std::size_t c_ContentLength = offset < a_TokenIds.size()
			? std::min(c_MaxContentLength, a_TokenIds.size() - offset)
			: 0;
		chunks.m_InputIds.emplace_back(c_ClsId);
		chunks.m_AttentionMask.emplace_back(1);
		chunks.m_InputIds.insert(
			chunks.m_InputIds.end(),
			a_TokenIds.begin() + offset,
			a_TokenIds.begin() + offset + c_ContentLength);
		chunks.m_AttentionMask.insert(
			chunks.m_AttentionMask.end(), c_ContentLength, 1);
		chunks.m_InputIds.emplace_back(c_SepId);
		chunks.m_AttentionMask.emplace_back(1);

		const std::size_t c_PaddingLength =
			GugluuConstants::c_MaxSequenceLength
			- c_ContentLength - GugluuConstants::c_SpecialTokenCount;
		chunks.m_InputIds.insert(
			chunks.m_InputIds.end(), c_PaddingLength, c_PadId);
		chunks.m_AttentionMask.insert(
			chunks.m_AttentionMask.end(), c_PaddingLength, 0);
	}

	return chunks;
}

TokenizedText Tokenizer::Tokenize(const std::string& a_InputString)
{
	static Tokenizer s_Tokenizer(UtilsGugluu::ResourcePath(
		GugluuConstants::c_VocabularyFilePath));
	return s_Tokenizer.TokenizeText(a_InputString);
}
