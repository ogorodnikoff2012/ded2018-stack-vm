#include <ram.h>
#include <algorithm>

RAM::RAM() {
    std::fill(chunk_table_.begin(), chunk_table_.end(), nullptr);
}

RAM::~RAM() {
    Resize(-1);
    while (pool_size_ > 0) {
        delete ExtractFromPool();
    }
}

int64_t* RAM::At(int64_t idx, bool* ok) {
    int chunk_idx = idx >> kChunkSizeLog;
    if (chunk_idx >= chunks_cnt_) {
        *ok = false;
        return nullptr;
    }
    if (chunk_table_[chunk_idx] == nullptr) {
        chunk_table_[chunk_idx] = AllocateChunk();
    }
    *ok = true;
    return &chunk_table_[chunk_idx]->at(idx & (kChunkSize - 1));
}

bool RAM::Resize(int64_t max_idx) {
    int new_chunks_cnt = 0;
    if (max_idx >= 0) {
        new_chunks_cnt = (max_idx >> kChunkSizeLog) + 1;
    }
    if (new_chunks_cnt > kMaxChunksCnt) {
        return false;
    }

    if (new_chunks_cnt > chunks_cnt_) {
        chunks_cnt_ = new_chunks_cnt;
        if (chunks_cnt_ + pool_size_ > kMaxChunksCnt) {
            pool_size_ = kMaxChunksCnt - chunks_cnt_;
        }
    } else {
        while (chunks_cnt_ > new_chunks_cnt) {
            --chunks_cnt_;
            Chunk* chunk = chunk_table_[chunks_cnt_];
            chunk_table_[chunks_cnt_] = nullptr;
            if (chunk != nullptr) {
                InsertInPool(chunk);
            }
        }
    }
    return true;
}

RAM::Chunk* RAM::AllocateChunk() {
    if (pool_size_ > 0) {
        return ExtractFromPool();
    }
    return new Chunk;
}

RAM::Chunk* RAM::ExtractFromPool() {
    Chunk* chunk = chunk_table_[kMaxChunksCnt - pool_size_];
    chunk_table_[kMaxChunksCnt - pool_size_] = nullptr;
    --pool_size_;
    return chunk;
}

void RAM::InsertInPool(RAM::Chunk* chunk) {
    chunk_table_[kMaxChunksCnt - (++pool_size_)] = chunk;
}
