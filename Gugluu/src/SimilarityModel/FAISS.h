#pragma once

#include "Gugluu/src/core/Constants.h"

#include <faiss/IndexFlat.h>
#include <vector>
#include <cstddef>


struct SimilarityResult
{
    std::size_t ChunkIndex;
    float Similarity;
};

class FAISS
{
public:
    static void s_Initialize(
        const std::vector<float>& a_Embeddings
    );

    static std::vector<SimilarityResult> s_GetSimilarityMap(
        const std::vector<float>& a_QueryEmbedding,
        std::size_t a_TopK = 500
    );

private:
    FAISS() = default;

    static FAISS& s_GetInstance();

    void Initialize(const std::vector<float>& a_Embeddings);

    std::vector<SimilarityResult> GetSimilarityMap(
        const std::vector<float>& a_QueryEmbedding,
        std::size_t a_TopK
    ) const;

private:
    faiss::IndexFlatIP m_Index{
        static_cast<faiss::idx_t>(GugluuConstants::c_EmbeddingDimensions) };
    bool m_Initialized = false;
};