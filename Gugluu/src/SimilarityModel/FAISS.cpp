#include "Gugluu/src/core/pch.h"
#include "FAISS.h"

namespace
{
    constexpr std::size_t c_EmbeddingDimension =
        GugluuConstants::c_EmbeddingDimensions;

    void Normalize(float* a_Embedding, std::size_t a_Dimensions)
    {
        GUGLUU_ASSERT(
            a_Embedding != nullptr && a_Dimensions != 0,
            "Cannot normalize an empty embedding.");

        double c_NormSquared = 0.0;
        for (std::size_t i = 0; i < a_Dimensions; ++i)
        {
            const float c_Value = a_Embedding[i];
            GUGLUU_ASSERT(
                std::isfinite(c_Value),
                "Embedding contains a non-finite value.");
            c_NormSquared += static_cast<double>(c_Value) * c_Value;
        }

        GUGLUU_ASSERT(
            std::isfinite(c_NormSquared) && c_NormSquared > 0.0,
            "Cannot normalize a zero-length embedding.");

        const double c_Norm = std::sqrt(c_NormSquared);
        GUGLUU_ASSERT(
            std::isfinite(c_Norm) && c_Norm > 0.0,
            "Embedding norm is invalid.");

        for (std::size_t i = 0; i < a_Dimensions; ++i)
            a_Embedding[i] = static_cast<float>(a_Embedding[i] / c_Norm);
    }
}

FAISS& FAISS::s_GetInstance()
{
    static FAISS s_Instance;
    return s_Instance;
}

void FAISS::s_Initialize(const std::vector<float>& a_Embeddings)
{
    s_GetInstance().Initialize(a_Embeddings);
}

std::vector<SimilarityResult> FAISS::s_GetSimilarityMap(
    const std::vector<float>& a_QueryEmbedding,
    std::size_t a_TopK)
{
    return s_GetInstance().GetSimilarityMap(a_QueryEmbedding, a_TopK);
}

void FAISS::Initialize(const std::vector<float>& a_Embeddings)
{
    GUGLUU_ASSERT(
        a_Embeddings.size() % c_EmbeddingDimension == 0,
        "Embedding array size is not divisible by the embedding dimension.");

    const std::size_t c_ChunkCount =
        a_Embeddings.size() / c_EmbeddingDimension;
    GUGLUU_ASSERT(
        c_ChunkCount <= static_cast<std::size_t>(
            (std::numeric_limits<faiss::idx_t>::max)()),
        "Embedding count exceeds FAISS's supported index range.");

    std::vector<float> c_NormalizedEmbeddings = a_Embeddings;
    for (std::size_t i = 0; i < c_NormalizedEmbeddings.size();
        i += c_EmbeddingDimension)
    {
        Normalize(
            c_NormalizedEmbeddings.data() + i,
            c_EmbeddingDimension);
    }

    faiss::IndexFlatIP c_NewIndex(
        static_cast<faiss::idx_t>(c_EmbeddingDimension));
    if (c_ChunkCount > 0)
    {
        c_NewIndex.add(
            static_cast<faiss::idx_t>(c_ChunkCount),
            c_NormalizedEmbeddings.data());
    }

    m_Index = std::move(c_NewIndex);
    m_Initialized = true;
}

std::vector<SimilarityResult> FAISS::GetSimilarityMap(
    const std::vector<float>& a_QueryEmbedding,
    std::size_t a_TopK) const
{
    GUGLUU_ASSERT(m_Initialized, "FAISS has not been initialized.");
    GUGLUU_ASSERT(
        a_QueryEmbedding.size() == c_EmbeddingDimension,
        "Query embedding has an invalid dimension.");

    if (a_TopK == 0 || m_Index.ntotal == 0)
        return {};

    std::vector<float> c_Query = a_QueryEmbedding;
    Normalize(c_Query.data(), c_Query.size());

    const std::size_t c_ResultCount = (std::min)(
        a_TopK,
        static_cast<std::size_t>(m_Index.ntotal));
    std::vector<float> c_Scores(c_ResultCount);
    std::vector<faiss::idx_t> c_Labels(c_ResultCount);

    m_Index.search(
        1,
        c_Query.data(),
        static_cast<faiss::idx_t>(c_ResultCount),
        c_Scores.data(),
        c_Labels.data());

    std::vector<SimilarityResult> c_Results;
    c_Results.reserve(c_ResultCount);

    for (std::size_t i = 0; i < c_ResultCount; ++i)
    {
        if (c_Labels[i] < 0)
            continue;

        GUGLUU_ASSERT(
            c_Labels[i] < m_Index.ntotal && std::isfinite(c_Scores[i]),
            "FAISS returned an invalid search result.");
        c_Results.push_back({
            static_cast<std::size_t>(c_Labels[i]),
            c_Scores[i]});
    }

    return c_Results;
}