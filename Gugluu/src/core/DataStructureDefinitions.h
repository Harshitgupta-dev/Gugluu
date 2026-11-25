#pragma once

#include <cstdint>
#include <vector>

#include "Gugluu/src/core/Macros.h"
#include "Gugluu/src/FileEmbeddings/FileRecord.h"

struct TokenizedText
{
    std::vector<std::int64_t> InputIds;
    std::vector<std::int64_t> AttentionMask;
};

inline std::ostream& operator<<(
    std::ostream& a_Os,
    const TokenizedText& a_Text)
{
    a_Os << "input IDs: " << a_Text.InputIds
        << " attention mask: " << a_Text.AttentionMask;

    return a_Os;
}

struct ChunkEmbeddings
{
    std::vector<float> embeddings;
    std::vector<std::uint64_t> owner;
};
