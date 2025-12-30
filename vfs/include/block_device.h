#pragma once
#include <cstdint>
#include <memory>

struct IBlockDevice {
  virtual ~IBlockDevice() = default;

  virtual uint32_t BlockSize() const = 0;
  virtual uint32_t NumBlocks() const = 0;

  virtual bool ReadBlock(uint32_t block_id, void* out) = 0;
  virtual bool WriteBlock(uint32_t block_id, const void* data) = 0;

  virtual bool Flush() = 0;
};

// Factory for memory-based block device (for testing)
std::shared_ptr<IBlockDevice> MakeMemBlockDevice(uint32_t num_blocks, uint32_t block_size);