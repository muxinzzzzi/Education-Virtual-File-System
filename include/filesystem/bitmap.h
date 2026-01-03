#ifndef BITMAP_H
#define BITMAP_H

#include <cstdint>
#include <mutex>
#include <vector>

namespace vfs {

/**
 * @brief Free block bitmap manager
 * Manages allocation and deallocation of data blocks
 */
class Bitmap {
public:
  explicit Bitmap(uint32_t total_blocks);

  // Allocate a free block
  int32_t allocate();

  // Free a block
  bool free(uint32_t block_num);

  // Check if a block is allocated
  bool is_allocated(uint32_t block_num) const;

  // Get number of free blocks
  uint32_t get_free_count() const;

  // Serialize to bytes
  std::vector<uint8_t> serialize() const;

  // Deserialize from bytes
  bool deserialize(const std::vector<uint8_t> &data);

  // Get bitmap size in bytes
  size_t size() const { return bitmap_.size(); }

private:
  std::vector<uint8_t> bitmap_;
  uint32_t total_blocks_;
  uint32_t free_blocks_;
  mutable std::mutex mutex_;

  // Helper functions
  void set_bit(uint32_t pos);
  void clear_bit(uint32_t pos);
  bool get_bit(uint32_t pos) const;
};

} // namespace vfs

#endif // BITMAP_H
