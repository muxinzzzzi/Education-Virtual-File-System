#include "block_device.h"
#include <vector>
#include <cstring>
#include <stdexcept>
#include <memory>

class MemBlockDevice final : public IBlockDevice {
public:
  MemBlockDevice(uint32_t num_blocks, uint32_t block_size)
      : num_blocks_(num_blocks),
        block_size_(block_size),
        data_(static_cast<size_t>(num_blocks) * block_size, 0) {
    if (num_blocks_ == 0 || block_size_ == 0) {
      throw std::runtime_error("bad block device size");
    }
  }

  uint32_t BlockSize() const override { return block_size_; }
  uint32_t NumBlocks() const override { return num_blocks_; }

  bool ReadBlock(uint32_t block_id, void* out) override {
    if (!out || block_id >= num_blocks_) return false;
    std::memcpy(out, &data_[static_cast<size_t>(block_id) * block_size_], block_size_);
    return true;
  }

  bool WriteBlock(uint32_t block_id, const void* in) override {
    if (!in || block_id >= num_blocks_) return false;
    std::memcpy(&data_[static_cast<size_t>(block_id) * block_size_], in, block_size_);
    return true;
  }

  bool Flush() override { return true; }

private:
  uint32_t num_blocks_;
  uint32_t block_size_;
  std::vector<uint8_t> data_;
};

std::shared_ptr<IBlockDevice> MakeMemBlockDevice(uint32_t num_blocks, uint32_t block_size) {
  return std::make_shared<MemBlockDevice>(num_blocks, block_size);
}
