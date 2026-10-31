#pragma once

#include <cstdint>
#include <vector>

struct TokenizedText
{
	std::vector<std::int64_t> m_InputIds;
	std::vector<std::int64_t> m_AttentionMask;
};
