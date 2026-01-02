#include "filesystem/lru_cache.h"
#include <algorithm>

namespace vfs {

LRUCache::LRUCache(size_t capacity)
    : capacity_(capacity), hits_(0), misses_(0), evictions_(0) {}

bool LRUCache::get(uint32_t block_num, std::vector<char> &data) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = cache_map_.find(block_num);
  if (it == cache_map_.end()) {
    misses_++;
    return false;
  }

  // Move to front of LRU list
  lru_list_.erase(it->second.first);
  lru_list_.push_front(block_num);
  it->second.first = lru_list_.begin();

  data = it->second.second;
  hits_++;
  return true;
}

void LRUCache::put(uint32_t block_num, const std::vector<char> &data) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = cache_map_.find(block_num);
  if (it != cache_map_.end()) {
    // Update existing entry
    lru_list_.erase(it->second.first);
    lru_list_.push_front(block_num);
    it->second.first = lru_list_.begin();
    it->second.second = data;
    return;
  }

  // Check if cache is full
  if (cache_map_.size() >= capacity_) {
    // Evict least recently used
    uint32_t lru_block = lru_list_.back();
    lru_list_.pop_back();
    cache_map_.erase(lru_block);
    evictions_++;
  }

  // Add new entry
  lru_list_.push_front(block_num);
  cache_map_[block_num] = {lru_list_.begin(), data};
}

void LRUCache::invalidate(uint32_t block_num) {
  std::lock_guard<std::mutex> lock(mutex_);

  auto it = cache_map_.find(block_num);
  if (it != cache_map_.end()) {
    lru_list_.erase(it->second.first);
    cache_map_.erase(it);
  }
}

void LRUCache::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  lru_list_.clear();
  cache_map_.clear();
}

CacheStats LRUCache::get_stats() const {
  std::lock_guard<std::mutex> lock(mutex_);

  CacheStats stats;
  stats.hits = hits_;
  stats.misses = misses_;
  stats.evictions = evictions_;
  stats.total_requests = hits_ + misses_;

  return stats;
}

void LRUCache::set_capacity(size_t new_capacity) {
  std::lock_guard<std::mutex> lock(mutex_);

  capacity_ = new_capacity;

  // Evict excess entries if necessary
  while (cache_map_.size() > capacity_) {
    uint32_t lru_block = lru_list_.back();
    lru_list_.pop_back();
    cache_map_.erase(lru_block);
    evictions_++;
  }
}

size_t LRUCache::get_size() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return cache_map_.size();
}

} // namespace vfs
