#pragma once

#include <array>

class RAM {
public:
    static constexpr int kChunkSizeLog = 9;
    static constexpr int kMaxChunksCntLog = 9;
    static constexpr int kChunkSize = (1 << kChunkSizeLog);
    static constexpr int kMaxChunksCnt = (1 << kMaxChunksCntLog);
    static constexpr int kInitialChunksCnt = 16;
    using Chunk = std::array<int64_t, kChunkSize>;

    RAM();
    ~RAM();
    int64_t* At(int64_t idx, bool* ok);
    bool Resize(int64_t max_idx);

private:
    Chunk* AllocateChunk();
    Chunk* ExtractFromPool();
    void InsertInPool(Chunk* chunk);
    int chunks_cnt_ = kInitialChunksCnt;
    int pool_size_ = 0;
    std::array<Chunk*, kMaxChunksCnt> chunk_table_;
};
