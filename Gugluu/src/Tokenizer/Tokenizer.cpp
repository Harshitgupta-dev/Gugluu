#include "Gugluu/src/core/pch.h"
#include "Gugluu/src/Tokenizer/Tokenizer.h"


Tokenizer::Tokenizer(
	const std::filesystem::path& a_FilePath)
{
	LoadVocab(a_FilePath);
}


void Tokenizer::LoadVocab(
	const std::filesystem::path& a_FilePath)
{
	std::ifstream vocab(a_FilePath);

	GUGLUU_ASSERT(
		vocab.is_open(),
		"Failed to load vocabulary file: "
		<< a_FilePath.string());


	std::string line;
	std::int64_t index = 0;

	while (std::getline(vocab, line))
	{
		GUGLUU_ASSERT(
			!line.empty(),
			"The vocabulary contains an empty token.");
		GUGLUU_ASSERT(
			m_Vocabulary.emplace(std::move(line), index++).second,
			"The vocabulary contains a duplicate token.");
	}

	GUGLUU_ASSERT(!vocab.bad(), "Failed while reading the vocabulary file.");
	GUGLUU_ASSERT(
		!m_Vocabulary.empty(),
		"The vocabulary is empty.");


	auto clsIterator =
		m_Vocabulary.find("[CLS]");

	auto sepIterator =
		m_Vocabulary.find("[SEP]");

	auto padIterator =
		m_Vocabulary.find("[PAD]");

	auto unknownIterator =
		m_Vocabulary.find("[UNK]");

	GUGLUU_ASSERT(
		clsIterator != m_Vocabulary.end() &&
		sepIterator != m_Vocabulary.end() &&
		padIterator != m_Vocabulary.end() &&
		unknownIterator != m_Vocabulary.end(),
		"The vocabulary is missing a required special token.");


	m_CLSId = clsIterator->second;
	m_SEPId = sepIterator->second;
	m_PADId = padIterator->second;
}


TokenizedText Tokenizer::TokenizeText(
	std::string& a_InputString)
{
	Normalize(a_InputString);

	std::vector<std::string> preTokens =
		PreTokenize(a_InputString);


	std::vector<std::string> tokens;

	tokens.reserve(
		preTokens.size() * 2);


	for (const std::string& token : preTokens)
	{
		if (token == "\n")
		{
			tokens.emplace_back("\n");
			continue;
		}


		WordPieceTokenize(
			token,
			tokens);
	}


	TokenizedText result;

	ChunkAndConvert(
		tokens,
		result);

	const std::size_t c_SequenceLength =
		GugluuConstants::c_MaxSequenceLength;

	GUGLUU_ASSERT(
		result.InputIds.size() == result.AttentionMask.size()
		&& result.InputIds.size() % c_SequenceLength == 0,
		"Tokenized input has an invalid shape.");

	return result;
}


void Tokenizer::Normalize(
	std::string& a_InputString)
{
	std::string normalized;

	normalized.reserve(
		a_InputString.size());


	bool previousWasNewline = false;


	for (unsigned char c : a_InputString)
	{
		if (c == '\n')
		{
			if (!previousWasNewline)
			{
				normalized.push_back('\n');
				previousWasNewline = true;
			}

			continue;
		}


		previousWasNewline = false;


		if (std::isspace(c))
		{
			if (!normalized.empty() &&
				normalized.back() != ' ' &&
				normalized.back() != '\n')
			{
				normalized.push_back(' ');
			}

			continue;
		}


		normalized.push_back(
			static_cast<char>(
				std::tolower(c)));
	}


	a_InputString = std::move(normalized);
}


std::vector<std::string> Tokenizer::PreTokenize(
	const std::string& a_InputString)
{
	std::vector<std::string> tokens;

	tokens.reserve(
		a_InputString.size() / 4 + 1);


	std::string currentToken;

	currentToken.reserve(32);


	for (unsigned char c : a_InputString)
	{
		if (c == '\n')
		{
			if (!currentToken.empty())
			{
				tokens.emplace_back(
					std::move(currentToken));

				currentToken.clear();
			}

			tokens.emplace_back("\n");

			continue;
		}


		if (std::isspace(c))
		{
			if (!currentToken.empty())
			{
				tokens.emplace_back(
					std::move(currentToken));

				currentToken.clear();
			}

			continue;
		}


		if (std::ispunct(c))
		{
			if (!currentToken.empty())
			{
				tokens.emplace_back(
					std::move(currentToken));

				currentToken.clear();
			}


			tokens.emplace_back(
				1,
				static_cast<char>(c));

			continue;
		}


		currentToken.push_back(
			static_cast<char>(c));
	}


	if (!currentToken.empty())
	{
		tokens.emplace_back(
			std::move(currentToken));
	}


	return tokens;
}


void Tokenizer::WordPieceTokenize(
	const std::string& a_Word,
	std::vector<std::string>& a_Output)
{
	if (a_Word.empty())
		return;


	if (a_Word.length() >
		GugluuConstants::c_MaxTokenCharacters)
	{
		a_Output.emplace_back("[UNK]");
		return;
	}

	if (m_Vocabulary.find(a_Word)
		!= m_Vocabulary.end())
	{
		a_Output.emplace_back(a_Word);
		return;
	}


	size_t start = 0;


	std::string candidate;

	candidate.reserve(
		GugluuConstants::c_MaxTokenCharacters + 2);


	while (start < a_Word.length())
	{
		size_t end =
			a_Word.length();

		bool found = false;
		while (start < end)
		{
			std::string_view piece =
				std::string_view(
					a_Word.data() + start,
					end - start);


			if (start == 0)
			{
				if (m_Vocabulary.find(piece)
					!= m_Vocabulary.end())
				{
					a_Output.emplace_back(piece);

					start = end;
					found = true;

					break;
				}
			}
			else
			{
				candidate.clear();

				candidate.append("##");
				candidate.append(piece);


				if (m_Vocabulary.find(candidate)
					!= m_Vocabulary.end())
				{
					a_Output.emplace_back(
						std::move(candidate));

					candidate.clear();

					start = end;
					found = true;

					break;
				}
			}


			--end;
		}


		if (!found)
		{
			a_Output.emplace_back("[UNK]");
			return;
		}
	}
}

void Tokenizer::ChunkAndConvert(
	const std::vector<std::string>& a_Tokens,
	TokenizedText& a_Result)
{
	const std::size_t c_MaxSequenceLength =
		GugluuConstants::c_MaxSequenceLength;

	const std::size_t c_SpecialTokenCount =
		GugluuConstants::c_SpecialTokenCount;

	const std::size_t c_MaxContentLength =
		c_MaxSequenceLength - c_SpecialTokenCount;

	std::vector<std::int64_t> c_CurrentChunk;
	c_CurrentChunk.reserve(c_MaxContentLength);

	auto EmitChunk =
		[&]()
		{
			if (c_CurrentChunk.empty())
				return;

			a_Result.InputIds.emplace_back(m_CLSId);
			a_Result.AttentionMask.emplace_back(1);

			for (const std::int64_t c_TokenId : c_CurrentChunk)
			{
				a_Result.InputIds.emplace_back(c_TokenId);
				a_Result.AttentionMask.emplace_back(1);
			}

			a_Result.InputIds.emplace_back(m_SEPId);
			a_Result.AttentionMask.emplace_back(1);

			while (a_Result.InputIds.size()
				% c_MaxSequenceLength != 0)
			{
				a_Result.InputIds.emplace_back(m_PADId);
				a_Result.AttentionMask.emplace_back(0);
			}

			GUGLUU_ASSERT(
				a_Result.InputIds.size()
				== a_Result.AttentionMask.size(),
				"Input IDs and attention mask have different sizes.");

			c_CurrentChunk.clear();
		};

	auto AddToken =
		[&](const std::int64_t a_TokenId)
		{
			if (c_CurrentChunk.size() >= c_MaxContentLength)
				EmitChunk();

			c_CurrentChunk.emplace_back(a_TokenId);
		};


	for (const std::string& c_Token : a_Tokens)
	{
		if (c_Token == "\n")
		{
			EmitChunk();
			continue;
		}

		const std::int64_t c_TokenId =
			GetTokenId(c_Token);

		AddToken(c_TokenId);

		// End the chunk at a sentence boundary.
		if (c_Token == ".")
		{
			EmitChunk();
		}
	}

	EmitChunk();
}

std::int64_t Tokenizer::GetTokenId(
	std::string_view a_Token) const
{
	auto iterator =
		m_Vocabulary.find(a_Token);


	GUGLUU_ASSERT(
		iterator != m_Vocabulary.end(),
		"Token does not exist in vocabulary: "
		<< a_Token);


	return iterator->second;
}


TokenizedText Tokenizer::s_Tokenize(
	const std::string& a_InputString)
{
	static Tokenizer s_Tokenizer(
		UtilsGugluu::ResourcePath(
			GugluuConstants::c_VocabularyFilePath));


	std::string input =
		a_InputString;

	return s_Tokenizer.TokenizeText(
		input);
}