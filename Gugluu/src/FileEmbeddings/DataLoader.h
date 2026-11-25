#pragma once

#include "Gugluu/src/core/pch.h"

class DataLoader
{
public:

    struct Data
    {
        const std::vector<FileRecord>& files;
        const ChunkEmbeddings& chunkEmbeddings;
        bool embeddingsChanged;
    };

    using InitializeCallback =
        std::function<void(const std::vector<float>&)>;

    using CreateEmbeddingsCallback =
        std::function<std::vector<float>(const TokenizedText&)>;

    using TokenizeCallback =
        std::function<TokenizedText(const std::string&)>;


    // Bind the external processing functions.
    static void s_Bind(
        InitializeCallback a_Initialize,
        CreateEmbeddingsCallback a_CreateEmbeddings,
        TokenizeCallback a_Tokenize);

    // Updates the index, saves it, and returns all active data.
    static Data s_GetData(
        const std::filesystem::path& a_RootDirectory);


private:

    struct PendingFile
    {
        std::filesystem::path path;
        std::uint64_t id = 0;
        std::uint64_t oldChunkId = 0;
        std::uint64_t tokenChunkStart = 0;
        std::uint64_t tokenChunkCount = 0;
        bool isNew = false;
    };


    explicit DataLoader(std::filesystem::path a_RootDirectory);

    static DataLoader& s_GetLoader(
        const std::filesystem::path& a_RootDirectory);


    void UpdateData();
    void LoadAllowedFileTypes();
    void LoadData();
    void SaveData() const;

    bool IsAllowedFileType(
        const std::filesystem::path& a_FilePath) const;

    void ReplaceChunks(
        FileRecord& a_Record,
        std::vector<float> a_Embeddings);

    void RecomputeChunkIds();


    // External callbacks.
    inline static InitializeCallback s_InitializeCallback;
    inline static CreateEmbeddingsCallback s_CreateEmbeddingsCallback;
    inline static TokenizeCallback s_TokenizeCallback;


    std::filesystem::path m_RootDirectory;

    std::vector<FileRecord> m_Files;
    std::vector<std::string> m_AllowedFileTypes;

    ChunkEmbeddings m_ChunkEmbeddings;

    std::uint64_t m_NextFileId = 0;

    // True whenever the embedding storage needs to be synchronized with FAISS.
    bool m_EmbeddingsChanged = true;
};