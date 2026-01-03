#include "filesystem/bitmap.h"
#include <cstring>
#include <stdexcept>

namespace vfs {

Bitmap::Bitmap(uint32_t total_blocks)
    : total_blocks_(total_blocks), free_blocks_(total_blocks) {
  // Calculate bitmap size in bytes (round up)
  size_t bitmap_size = (total_blocks + 7) / 8;
  bitmap_.resize(bitmap_size, 0);
}

int32_t Bitmap::allocate() {
  std::lock_guard<std::mutex> lock(mutex_);

  if (free_blocks_ == 0) {
    return -1; // No free blocks
  }

  // Find first free block
  for (uint32_t i = 0; i < total_blocks_; ++i) {
    if (!get_bit(i)) {
      set_bit(i);
      free_blocks_--;
      return static_cast<int32_t>(i);
    }
  }

  return -1;
}

bool Bitmap::free(uint32_t block_num) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (block_num >= total_blocks_) {
    return false;
  }

  if (!get_bit(block_num)) {
    return false; // Already free
  }

  clear_bit(block_num);
  free_blocks_++;
  return true;
}

bool Bitmap::is_allocated(uint32_t block_num) const {
  std::lock_guard<std::mutex> lock(mutex_);

  if (block_num >= total_blocks_) {
    return false;
  }

  return get_bit(block_num);
}

uint32_t Bitmap::get_free_count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return free_blocks_;
}

std::vector<uint8_t> Bitmap::serialize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return bitmap_;
}

bool Bitmap::deserialize(const std::vector<uint8_t> &data) {
  std::lock_guard<std::mutex> lock(mutex_);

  if (data.size() != bitmap_.size()) {
    return false;
  }

  bitmap_ = data;

  // Recalculate free blocks
  free_blocks_ = 0;
  for (uint32_t i = 0; i < total_blocks_; ++i) {
    if (!get_bit(i)) {
      free_blocks_++;
    }
  }

  return true;
}

void Bitmap::set_bit(uint32_t pos) {
  size_t byte_index = pos / 8;
  size_t bit_index = pos % 8;
  bitmap_[byte_index] |= (1 << bit_index);
}

void Bitmap::clear_bit(uint32_t pos) {
  size_t byte_index = pos / 8;
  size_t bit_index = pos % 8;
  bitmap_[byte_index] &= ~(1 << bit_index);
}

bool Bitmap::get_bit(uint32_t pos) const {
  size_t byte_index = pos / 8;
  size_t bit_index = pos % 8;
  return (bitmap_[byte_index] & (1 << bit_index)) != 0;
}

} // namespace vfs
