#include "filesystem/vfs.h"
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
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

  try {
    std::filesystem::remove(image_path + ".checksum");
    std::filesystem::remove(image_path + ".journal");
  } catch (...) {
  }

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
  journal_path_ = image_path_ + ".journal";
  checksum_path_ = image_path_ + ".checksum";
  load_checksums();
  replay_journal();
  load_snapshots();
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

  save_checksums();
  flush_and_clear_journal();

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

  if (block_num < block_checksums_.size() && block_checksums_[block_num] != 0) {
    uint32_t expect = block_checksums_[block_num];
    uint32_t got = calc_checksum(data);
    if (expect != got) {
      std::cerr << "[VFS WARN] Checksum mismatch on block " << block_num
                << " expect " << expect << " got " << got << "\n";
    }
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

  // Capture original block for snapshots
  std::vector<char> original;
  if (!snapshots_.empty()) {
    read_block(block_num, original);
  }

  append_journal_entry(block_num, data);

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

  if (block_num < block_checksums_.size()) {
    block_checksums_[block_num] = calc_checksum(data);
  }

  if (!snapshots_.empty()) {
    snapshot_record_block(block_num, original);
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

VirtualFileSystem::JournalStats VirtualFileSystem::get_journal_stats() const {
  return journal_stats_;
}

uint32_t VirtualFileSystem::calc_checksum(const std::vector<char> &data) const {
  uint32_t h = 0;
  for (unsigned char c : data) {
    h = (h * 131) + c;
  }
  return h;
}

void VirtualFileSystem::load_checksums() {
  block_checksums_.assign(superblock_.total_blocks, 0);
  if (checksum_path_.empty()) {
    return;
  }
  std::ifstream in(checksum_path_, std::ios::binary);
  if (!in) {
    return;
  }
  in.read(reinterpret_cast<char *>(block_checksums_.data()),
          block_checksums_.size() * sizeof(uint32_t));
}

void VirtualFileSystem::save_checksums() {
  if (checksum_path_.empty() || block_checksums_.empty()) {
    return;
  }
  std::ofstream out(checksum_path_, std::ios::binary | std::ios::trunc);
  if (!out) {
    return;
  }
  out.write(reinterpret_cast<const char *>(block_checksums_.data()),
            block_checksums_.size() * sizeof(uint32_t));
}

bool VirtualFileSystem::append_journal_entry(uint32_t block_num,
                                             const std::vector<char> &data) {
  if (journal_path_.empty()) {
    return false;
  }
  std::ofstream jf(journal_path_, std::ios::binary | std::ios::app);
  if (!jf) {
    return false;
  }
  uint32_t size = static_cast<uint32_t>(data.size());
  uint32_t checksum = calc_checksum(data);
  jf.write(reinterpret_cast<const char *>(&block_num), sizeof(block_num));
  jf.write(reinterpret_cast<const char *>(&size), sizeof(size));
  jf.write(reinterpret_cast<const char *>(&checksum), sizeof(checksum));
  jf.write(data.data(), data.size());
  jf.flush();
  journal_stats_.pending++;
  journal_stats_.dirty = true;
  return jf.good();
}

bool VirtualFileSystem::replay_journal() {
  if (journal_path_.empty()) {
    return true;
  }
  std::ifstream jf(journal_path_, std::ios::binary);
  if (!jf) {
    return true; // No journal to replay
  }

  while (jf) {
    uint32_t block_num = 0;
    uint32_t size = 0;
    uint32_t checksum = 0;
    jf.read(reinterpret_cast<char *>(&block_num), sizeof(block_num));
    jf.read(reinterpret_cast<char *>(&size), sizeof(size));
    jf.read(reinterpret_cast<char *>(&checksum), sizeof(checksum));
    if (!jf) {
      break;
    }
    if (size != BLOCK_SIZE) {
      break;
    }
    std::vector<char> data(size);
    jf.read(data.data(), size);
    if (!jf) {
      break;
    }
    if (calc_checksum(data) != checksum) {
      std::cerr << "[JOURNAL] checksum mismatch, skipping entry\n";
      continue;
    }
    uint64_t offset = static_cast<uint64_t>(block_num) * BLOCK_SIZE;
    image_file_.seekp(offset);
    image_file_.write(data.data(), BLOCK_SIZE);
    journal_stats_.replayed++;
  }

  image_file_.flush();
  flush_and_clear_journal();
  if (journal_stats_.replayed > 0) {
    journal_stats_.recovered = true;
  }
  return true;
}

bool VirtualFileSystem::flush_and_clear_journal() {
  if (journal_path_.empty()) {
    return true;
  }
  std::ofstream clear(journal_path_, std::ios::trunc);
  journal_stats_.pending = 0;
  journal_stats_.dirty = false;
  return clear.good();
}

void VirtualFileSystem::load_snapshots() {
  snapshots_.clear();
  if (image_path_.empty())
    return;
  try {
    std::filesystem::path img(image_path_);
    auto parent = img.parent_path().empty() ? std::filesystem::path(".")
                                            : img.parent_path();
    std::string base = img.filename().string();
    for (const auto &entry : std::filesystem::directory_iterator(parent)) {
      if (!entry.is_regular_file())
        continue;
      auto fname = entry.path().filename().string();
      std::string prefix = base + ".snap.";
      if (fname.rfind(prefix, 0) == 0 && fname.size() > prefix.size()) {
        if (fname.find(".diff", prefix.size()) != std::string::npos) {
          std::string snap_name =
              fname.substr(prefix.size(), fname.size() - prefix.size() - 5);
          SnapshotMeta meta;
          meta.name = snap_name;
          meta.diff_path = entry.path().string();
          meta.index_path = entry.path().replace_extension(".idx").string();
          // rebuild block list
          std::ifstream diff(meta.diff_path, std::ios::binary);
          while (diff) {
            uint32_t b = 0;
            diff.read(reinterpret_cast<char *>(&b), sizeof(b));
            if (!diff)
              break;
            std::vector<char> buf(BLOCK_SIZE);
            diff.read(buf.data(), BLOCK_SIZE);
            if (!diff)
              break;
            meta.blocks.insert(b);
          }
          snapshots_[snap_name] = std::move(meta);
        }
      }
    }
  } catch (const std::exception &e) {
    std::cerr << "[SNAPSHOT] load failed: " << e.what() << "\n";
  }
}

bool VirtualFileSystem::snapshot_record_block(
    uint32_t block_num, const std::vector<char> &original) {
  std::vector<char> data = original;
  if (data.size() != BLOCK_SIZE) {
    data.assign(BLOCK_SIZE, 0);
  }
  for (auto &kv : snapshots_) {
    auto &meta = kv.second;
    if (meta.blocks.find(block_num) != meta.blocks.end()) {
      continue;
    }
    std::ofstream diff(meta.diff_path, std::ios::binary | std::ios::app);
    if (!diff)
      continue;
    diff.write(reinterpret_cast<const char *>(&block_num), sizeof(block_num));
    diff.write(data.data(), BLOCK_SIZE);
    diff.flush();
    meta.blocks.insert(block_num);
  }
  return true;
}

// Continued in next part...

} // namespace vfs
