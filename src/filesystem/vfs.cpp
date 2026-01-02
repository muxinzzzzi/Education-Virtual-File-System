#include "filesystem/vfs.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <sys/stat.h>
#include <unistd.h>

namespace vfs {

VirtualFileSystem::VirtualFileSystem()
    : mounted_(false),
      next_fd_(3) { // Start from 3 (0,1,2 reserved for stdin/stdout/stderr)
}

VirtualFileSystem::~VirtualFileSystem() {
  if (mounted_) {
    unmount();
  }
}

bool VirtualFileSystem::format(const std::string &image_path, uint32_t size_mb,
                               size_t cache_capacity) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (mounted_) {
    return false;
  }

  uint64_t total_size = static_cast<uint64_t>(size_mb) * 1024 * 1024;
  uint32_t total_blocks = total_size / BLOCK_SIZE;
  uint32_t total_inodes = total_blocks / 8;
  if (total_inodes < 64)
    total_inodes = 64;

  uint32_t inode_blocks =
      (total_inodes * sizeof(Inode) + BLOCK_SIZE - 1) / BLOCK_SIZE;
  uint32_t bitmap_blocks =
      ((total_blocks + 7) / 8 + BLOCK_SIZE - 1) / BLOCK_SIZE;

  uint32_t inode_table_start = 1;
  uint32_t bitmap_start = inode_table_start + inode_blocks;
  uint32_t data_start = bitmap_start + bitmap_blocks;
  uint32_t data_blocks = total_blocks - data_start;

  // Initialize superblock
  superblock_ = Superblock();
  superblock_.magic = MAGIC_NUMBER;
  superblock_.total_blocks = total_blocks;
  superblock_.total_inodes = total_inodes;
  superblock_.free_blocks = data_blocks - 1;
  superblock_.free_inodes = total_inodes - 2;
  superblock_.inode_table_block = inode_table_start;
  superblock_.bitmap_block = bitmap_start;
  superblock_.data_block_start = data_start;
  superblock_.created_time = std::time(nullptr);
  superblock_.modified_time = superblock_.created_time;

  // 1. Create and physically fill the file
  std::ofstream ofs(image_path, std::ios::binary | std::ios::trunc);
  if (!ofs)
    return false;

  std::vector<char> zero_block(BLOCK_SIZE, 0);
  for (uint32_t i = 0; i < total_blocks; ++i) {
    ofs.write(zero_block.data(), BLOCK_SIZE);
  }
  ofs.flush();

  // 2. Write Superblock
  ofs.seekp(0);
  ofs.write(reinterpret_cast<const char *>(&superblock_), sizeof(Superblock));

  // 3. Prepare Dummy Inode 0 and Root Inode 1
  Inode null_inode;
  null_inode.inode_num = 0xDEADBEEF; // Mark it so we know it's not a zero block
  null_inode.mode = 0;

  Inode root;
  root.inode_num = 1;
  root.mode = S_IFDIR | 0755;
  root.size = 0;
  root.atime = root.mtime = root.ctime = std::time(nullptr);
  root.links_count = 2;
  root.blocks_count = 1;
  root.direct_blocks[0] = data_start;

  uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
  uint32_t root_block_num = inode_table_start + (1 / inodes_per_block);

  std::vector<char> inode_block_data(BLOCK_SIZE, 0);
  std::memcpy(inode_block_data.data() + (0 * sizeof(Inode)), &null_inode,
              sizeof(Inode));
  std::memcpy(inode_block_data.data() + (1 * sizeof(Inode)), &root,
              sizeof(Inode));

  ofs.seekp(root_block_num * BLOCK_SIZE);
  ofs.write(inode_block_data.data(), BLOCK_SIZE);

  // 4. Update bitmap for the first data block
  bitmap_ = std::make_unique<Bitmap>(data_blocks);
  bitmap_->allocate(); // Mark first data block as used
  auto bitmap_data = bitmap_->serialize();
  ofs.seekp(bitmap_start * BLOCK_SIZE);
  ofs.write(reinterpret_cast<const char *>(bitmap_data.data()),
            bitmap_data.size());

  ofs.flush();
  ofs.close();

  // --- SELF-VERIFICATION ---
  {
    std::ifstream check(image_path, std::ios::binary);
    if (check) {
      uint32_t check_val = 0;
      uint64_t verify_offset =
          static_cast<uint64_t>(root_block_num) * BLOCK_SIZE +
          (1 * sizeof(Inode));
      check.seekg(verify_offset);
      check.read(reinterpret_cast<char *>(&check_val), 4);
      std::cout << "[VFS VERIFY] After format, Inode 1 at offset "
                << verify_offset << " contains ID: " << check_val << "\n";
    }
  }

  std::cout << "[VFS] Format complete. " << size_mb << "MB image created.\n";

  // Release lock before calling mount to avoid deadlock
  lock.unlock();
  return mount(image_path, cache_capacity);
}

bool VirtualFileSystem::mount(const std::string &image_path,
                              size_t cache_capacity) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (mounted_) {
    return false;
  }

  // Open image file
  image_file_.open(image_path, std::ios::in | std::ios::out | std::ios::binary);
  if (!image_file_) {
    return false;
  }

  // Read superblock
  image_file_.seekg(0);
  image_file_.read(reinterpret_cast<char *>(&superblock_), sizeof(Superblock));

  if (superblock_.magic != MAGIC_NUMBER) {
    image_file_.close();
    return false;
  }

  // Calculate bitmap size
  uint32_t data_blocks =
      superblock_.total_blocks - superblock_.data_block_start;

  // Initialize bitmap
  bitmap_ = std::make_unique<Bitmap>(data_blocks);

  // Read bitmap from disk
  uint32_t bitmap_blocks =
      ((data_blocks + 7) / 8 + BLOCK_SIZE - 1) / BLOCK_SIZE;
  std::vector<uint8_t> bitmap_data;
  for (uint32_t i = 0; i < bitmap_blocks; ++i) {
    std::vector<char> block(BLOCK_SIZE);
    image_file_.seekg((superblock_.bitmap_block + i) * BLOCK_SIZE);
    image_file_.read(block.data(), BLOCK_SIZE);
    bitmap_data.insert(bitmap_data.end(), block.begin(), block.end());
  }
  bitmap_data.resize((data_blocks + 7) / 8);
  bitmap_->deserialize(bitmap_data);

  // Initialize cache
  cache_ = std::make_unique<LRUCache>(cache_capacity);

  image_path_ = image_path;
  mounted_ = true;

  return true;
}

void VirtualFileSystem::unmount() {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return;
  }

  // Write superblock
  superblock_.modified_time = std::time(nullptr);
  image_file_.seekp(0);
  image_file_.write(reinterpret_cast<const char *>(&superblock_),
                    sizeof(Superblock));

  // Write bitmap
  auto bitmap_data = bitmap_->serialize();
  uint32_t bitmap_blocks = (bitmap_data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
  for (uint32_t i = 0; i < bitmap_blocks; ++i) {
    image_file_.seekp((superblock_.bitmap_block + i) * BLOCK_SIZE);
    size_t offset = i * BLOCK_SIZE;
    size_t to_write =
        std::min(static_cast<size_t>(BLOCK_SIZE), bitmap_data.size() - offset);
    if (to_write > 0) {
      image_file_.write(
          reinterpret_cast<const char *>(bitmap_data.data() + offset),
          to_write);
    }
  }

  // Close file handles
  fd_table_.clear();
  image_file_.close();

  // Clear cache
  cache_->clear();

  mounted_ = false;
}

// Block-level I/O
bool VirtualFileSystem::read_block(uint32_t block_num,
                                   std::vector<char> &data) {
  if (data.size() != BLOCK_SIZE) {
    data.resize(BLOCK_SIZE);
  }

  // Check cache first
  if (cache_->get(block_num, data)) {
    return true;
  }

  // Read from disk
  uint64_t offset = static_cast<uint64_t>(block_num) * BLOCK_SIZE;
  image_file_.clear(); // CRITICAL: Clear error flags
  image_file_.seekg(offset);
  image_file_.read(data.data(), BLOCK_SIZE);

  if (!image_file_) {
    std::cerr << "[VFS ERROR] read_block: Failed to read block " << block_num
              << " at offset " << offset << "\n";
    return false;
  }

  // Update cache
  cache_->put(block_num, data);

  return true;
}

bool VirtualFileSystem::write_block(uint32_t block_num,
                                    const std::vector<char> &data) {
  if (data.size() != BLOCK_SIZE) {
    return false;
  }

  // Write to disk
  uint64_t offset = static_cast<uint64_t>(block_num) * BLOCK_SIZE;
  image_file_.clear(); // Clear any error flags
  image_file_.seekp(offset);
  image_file_.write(data.data(), BLOCK_SIZE);
  image_file_.flush(); // Force write to OS buffer

  if (!image_file_) {
    std::cerr << "[VFS ERROR] write_block: Failed to write block " << block_num
              << "\n";
    return false;
  }

  // Update cache
  cache_->put(block_num, data);

  return true;
}

// Inode operations
bool VirtualFileSystem::read_inode(uint32_t inode_num, Inode &inode) {
  if (inode_num >= superblock_.total_inodes) {
    std::cerr << "[VFS DEBUG] read_inode: Inode " << inode_num
              << " out of bounds (" << superblock_.total_inodes << ")\n";
    return false;
  }

  uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
  uint32_t block_num =
      superblock_.inode_table_block + (inode_num / inodes_per_block);
  uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);

  std::vector<char> block_data;
  if (!read_block(block_num, block_data)) {
    std::cerr << "[VFS DEBUG] read_inode: Failed to read block " << block_num
              << " for inode " << inode_num << "\n";
    return false;
  }

  std::memcpy(&inode, block_data.data() + offset_in_block, sizeof(Inode));
  return true;
}

bool VirtualFileSystem::write_inode(uint32_t inode_num, const Inode &inode) {
  if (inode_num >= superblock_.total_inodes) {
    return false;
  }

  uint32_t inodes_per_block = BLOCK_SIZE / sizeof(Inode);
  uint32_t block_num =
      superblock_.inode_table_block + (inode_num / inodes_per_block);
  uint32_t offset_in_block = (inode_num % inodes_per_block) * sizeof(Inode);

  std::vector<char> block_data;
  if (!read_block(block_num, block_data)) {
    return false;
  }

  std::memcpy(block_data.data() + offset_in_block, &inode, sizeof(Inode));

  if (!write_block(block_num, block_data)) {
    return false;
  }

  // std::cerr << "[VFS DEBUG] write_inode: Successfully wrote inode " <<
  // inode_num << " to block " << block_num << "\n";
  return true;
}

uint32_t VirtualFileSystem::allocate_inode() {
  // Find free inode, skip 0 (NULL) and 1 (ROOT)
  for (uint32_t i = 2; i < superblock_.total_inodes; ++i) {
    Inode inode;
    if (read_inode(i, inode) && inode.mode == 0) {
      if (superblock_.free_inodes > 0) {
        superblock_.free_inodes--;
      }
      return i;
    }
  }
  return static_cast<uint32_t>(-1);
}

bool VirtualFileSystem::free_inode(uint32_t inode_num) {
  Inode inode;
  std::memset(&inode, 0, sizeof(Inode));
  if (write_inode(inode_num, inode)) {
    superblock_.free_inodes++;
    return true;
  }
  return false;
}

uint32_t VirtualFileSystem::allocate_block() {
  int32_t block_num = bitmap_->allocate();
  if (block_num >= 0) {
    superblock_.free_blocks--;
    return superblock_.data_block_start + block_num;
  }
  return static_cast<uint32_t>(-1);
}

bool VirtualFileSystem::free_block(uint32_t block_num) {
  if (block_num < superblock_.data_block_start) {
    return false;
  }

  uint32_t data_block = block_num - superblock_.data_block_start;
  if (bitmap_->free(data_block)) {
    superblock_.free_blocks++;
    cache_->invalidate(block_num);
    return true;
  }
  return false;
}

void VirtualFileSystem::init_root_directory() {
  // 1. Initialize Inode 0 as NULL/Reserved
  Inode null_inode;
  null_inode.inode_num = 0;
  null_inode.mode = 0; // Invalid
  write_inode(0, null_inode);

  // 2. Initialize Inode 1 as ROOT
  Inode root;
  root.inode_num = 1;
  root.mode = S_IFDIR | S_IRWXU;
  root.uid = 0;
  root.gid = 0;
  root.size = 0;
  root.atime = root.mtime = root.ctime = std::time(nullptr);
  root.links_count = 2; // . and ..

  // Allocate the first block for root directory entries
  uint32_t block_num = allocate_block();
  if (block_num != static_cast<uint32_t>(-1)) {
    root.direct_blocks[0] = block_num;
    root.blocks_count = 1;

    // Initialize root directory block with zero entries
    std::vector<char> zero_block(BLOCK_SIZE, 0);
    write_block(block_num, zero_block);
  }

  write_inode(1, root);

  // 3. Update superblock to reflect reserved inodes
  superblock_.free_inodes = superblock_.total_inodes - 2;
  superblock_.modified_time = std::time(nullptr);

  // 4. Force write superblock and flush everything
  image_file_.seekp(0);
  image_file_.write(reinterpret_cast<const char *>(&superblock_),
                    sizeof(Superblock));
  image_file_.flush();

  std::cout << "[VFS] Root directory (Inode 1) initialized successfully.\n";
}

FileSystemStats VirtualFileSystem::get_fs_stats() const {
  std::shared_lock<std::shared_mutex> lock(fs_mutex_);

  FileSystemStats stats;
  stats.total_blocks = superblock_.total_blocks;
  stats.free_blocks = superblock_.free_blocks;
  stats.total_inodes = superblock_.total_inodes;
  stats.free_inodes = superblock_.free_inodes;
  stats.total_size =
      static_cast<uint64_t>(superblock_.total_blocks) * BLOCK_SIZE;
  stats.used_size = static_cast<uint64_t>(superblock_.total_blocks -
                                          superblock_.free_blocks) *
                    BLOCK_SIZE;

  return stats;
}

CacheStats VirtualFileSystem::get_cache_stats() const {
  return cache_->get_stats();
}

// Continued in next part...

} // namespace vfs
