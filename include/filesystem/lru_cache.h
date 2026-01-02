#ifndef LRU_CACHE_H
#define LRU_CACHE_H

#include "vfs_types.h"
#include <list>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace vfs {

/**
 * @brief LRU Cache for file system blocks
 * Thread-safe implementation with configurable capacity
 */
class LRUCache {
public:
  explicit LRUCache(size_t capacity);

  // Get block data from cache
  bool get(uint32_t block_num, std::vector<char> &data);

  // Put block data into cache
  void put(uint32_t block_num, const std::vector<char> &data);

  // Invalidate a block from cache
  void invalidate(uint32_t block_num);

  // Clear entire cache
  void clear();

  // Get cache statistics
  CacheStats get_stats() const;

  // Set capacity
  void set_capacity(size_t new_capacity);

  size_t get_capacity() const { return capacity_; }
  size_t get_size() const;

private:
  size_t capacity_;
  std::list<uint32_t> lru_list_; // Most recently used at front
  std::unordered_map<
      uint32_t, std::pair<std::list<uint32_t>::iterator, std::vector<char>>>
      cache_map_;

  // Statistics
  mutable uint64_t hits_;
  mutable uint64_t misses_;
  mutable uint64_t evictions_;

  mutable std::mutex mutex_;

  // Helper: move block to front of LRU list
  void touch(uint32_t block_num);

  // Helper: evict least recently used block
  void evict();
};

} // namespace vfs

#endif // LRU_CACHE_H
