#include "vfs.h"
#include <vector>
#include <cstring>
#include <algorithm>
#include <cmath>

static constexpr uint32_t kMagic   = 0x56534653; // 'VSFS'
static constexpr uint32_t kVersion = 1;
static constexpr uint32_t kInodeSize = 128;
static constexpr uint32_t kDirEntrySize = 64;
static constexpr uint32_t kDirectBlocks = 10;

#pragma pack(push, 1)
struct Vfs::SuperBlock {
  uint32_t magic;
  uint32_t version;
  uint32_t block_size;
  uint32_t num_blocks;
  uint32_t bmap_start;
  uint32_t bmap_blocks;
  uint32_t inode_start;
  uint32_t inode_blocks;
  uint32_t inode_capacity;
  uint32_t data_start;
  uint32_t root_inode;
  uint8_t  reserved[32];
};

struct Vfs::Inode {
  uint16_t mode;      // 0=free, 1=file, 2=dir
  uint16_t links;
  uint64_t size;
  uint32_t direct[kDirectBlocks];
  uint32_t indirect1;
  uint32_t indirect2;  // reserved
  uint8_t  reserved[68];  // Adjusted to make total 128 bytes
};

struct Vfs::DirEntry {
  uint32_t inode;     // 0 = empty slot
  char     name[60];
};
#pragma pack(pop)

Vfs::Vfs(std::shared_ptr<IBlockDevice> dev) : dev_(std::move(dev)) {
  sb_ = new SuperBlock();
}

Vfs::~Vfs() {
  delete sb_;
}

bool Vfs::Mkfs(std::shared_ptr<IBlockDevice> dev) {
  if (!dev) return false;

  uint32_t block_size = dev->BlockSize();
  uint32_t num_blocks = dev->NumBlocks();
  
  // Calculate layout
  uint32_t bmap_start = 1;
  uint32_t bmap_blocks = (num_blocks + block_size * 8 - 1) / (block_size * 8);
  
  uint32_t inode_start = bmap_start + bmap_blocks;
  uint32_t inode_capacity = 1024; // reasonable default
  uint32_t inode_blocks = (inode_capacity * kInodeSize + block_size - 1) / block_size;
  
  uint32_t data_start = inode_start + inode_blocks;
  
  if (data_start >= num_blocks) return false;

  // Write superblock
  SuperBlock sb{};
  sb.magic = kMagic;
  sb.version = kVersion;
  sb.block_size = block_size;
  sb.num_blocks = num_blocks;
  sb.bmap_start = bmap_start;
  sb.bmap_blocks = bmap_blocks;
  sb.inode_start = inode_start;
  sb.inode_blocks = inode_blocks;
  sb.inode_capacity = inode_capacity;
  sb.data_start = data_start;
  sb.root_inode = 0;
  std::memset(sb.reserved, 0, sizeof(sb.reserved));

  std::vector<uint8_t> buf(block_size, 0);
  std::memcpy(buf.data(), &sb, sizeof(sb));
  if (!dev->WriteBlock(0, buf.data())) return false;

  // Clear bitmap (all free initially)
  std::fill(buf.begin(), buf.end(), 0);
  for (uint32_t i = 0; i < bmap_blocks; ++i) {
    if (!dev->WriteBlock(bmap_start + i, buf.data())) return false;
  }
  
  // Mark metadata blocks as used
  for (uint32_t blk = 0; blk < data_start; ++blk) {
    uint32_t byte_idx = blk / 8;
    uint32_t bit_idx = blk % 8;
    uint32_t bmap_blk = bmap_start + byte_idx / block_size;
    uint32_t offset = byte_idx % block_size;
    
    if (!dev->ReadBlock(bmap_blk, buf.data())) return false;
    buf[offset] |= (1 << bit_idx);
    if (!dev->WriteBlock(bmap_blk, buf.data())) return false;
  }

  // Clear inode table
  std::fill(buf.begin(), buf.end(), 0);
  for (uint32_t i = 0; i < inode_blocks; ++i) {
    if (!dev->WriteBlock(inode_start + i, buf.data())) return false;
  }

  // Create root directory (inode 0)
  Inode root{};
  root.mode = 2; // directory
  root.links = 1;
  root.size = 0;
  std::memset(root.direct, 0, sizeof(root.direct));
  root.indirect1 = 0;
  root.indirect2 = 0;
  std::memset(root.reserved, 0, sizeof(root.reserved));
  
  std::memcpy(buf.data(), &root, sizeof(root));
  if (!dev->WriteBlock(inode_start, buf.data())) return false;

  return dev->Flush();
}

std::unique_ptr<Vfs> Vfs::Mount(std::shared_ptr<IBlockDevice> dev) {
  if (!dev) return nullptr;

  auto vfs = std::unique_ptr<Vfs>(new Vfs(std::move(dev)));
  if (!vfs->LoadSuperBlock()) return nullptr;

  return vfs;
}

bool Vfs::LoadSuperBlock() {
  std::vector<uint8_t> buf(dev_->BlockSize(), 0);
  if (!dev_->ReadBlock(0, buf.data())) return false;

  std::memcpy(sb_, buf.data(), sizeof(SuperBlock));

  if (sb_->magic != kMagic) return false;
  if (sb_->version != kVersion) return false;
  if (sb_->block_size != dev_->BlockSize()) return false;

  return true;
}

bool Vfs::ReadInode(uint32_t ino, Inode* out) {
  if (ino >= sb_->inode_capacity) return false;
  
  uint32_t inodes_per_block = sb_->block_size / kInodeSize;
  uint32_t block_id = sb_->inode_start + ino / inodes_per_block;
  uint32_t offset = (ino % inodes_per_block) * kInodeSize;
  
  std::vector<uint8_t> buf(sb_->block_size);
  if (!dev_->ReadBlock(block_id, buf.data())) return false;
  
  std::memcpy(out, buf.data() + offset, sizeof(Inode));
  return true;
}

bool Vfs::WriteInode(uint32_t ino, const Inode& in) {
  if (ino >= sb_->inode_capacity) return false;
  
  uint32_t inodes_per_block = sb_->block_size / kInodeSize;
  uint32_t block_id = sb_->inode_start + ino / inodes_per_block;
  uint32_t offset = (ino % inodes_per_block) * kInodeSize;
  
  std::vector<uint8_t> buf(sb_->block_size);
  if (!dev_->ReadBlock(block_id, buf.data())) return false;
  
  std::memcpy(buf.data() + offset, &in, sizeof(Inode));
  
  return dev_->WriteBlock(block_id, buf.data());
}

uint32_t Vfs::AllocInode() {
  for (uint32_t ino = 0; ino < sb_->inode_capacity; ++ino) {
    Inode inode;
    if (!ReadInode(ino, &inode)) continue;
    if (inode.mode == 0) {
      return ino;
    }
  }
  return UINT32_MAX;
}

bool Vfs::FreeInode(uint32_t ino) {
  Inode inode{};
  inode.mode = 0;
  return WriteInode(ino, inode);
}

uint32_t Vfs::AllocBlock() {
  std::vector<uint8_t> buf(sb_->block_size);
  
  for (uint32_t blk = sb_->data_start; blk < sb_->num_blocks; ++blk) {
    uint32_t byte_idx = blk / 8;
    uint32_t bit_idx = blk % 8;
    uint32_t bmap_blk = sb_->bmap_start + byte_idx / sb_->block_size;
    uint32_t offset = byte_idx % sb_->block_size;
    
    if (!dev_->ReadBlock(bmap_blk, buf.data())) continue;
    
    if ((buf[offset] & (1 << bit_idx)) == 0) {
      buf[offset] |= (1 << bit_idx);
      if (!dev_->WriteBlock(bmap_blk, buf.data())) return UINT32_MAX;
      
      // Clear the newly allocated block
      std::vector<uint8_t> zero(sb_->block_size, 0);
      dev_->WriteBlock(blk, zero.data());
      
      return blk;
    }
  }
  
  return UINT32_MAX;
}

bool Vfs::FreeBlock(uint32_t block_id) {
  if (block_id < sb_->data_start || block_id >= sb_->num_blocks) return false;
  
  uint32_t byte_idx = block_id / 8;
  uint32_t bit_idx = block_id % 8;
  uint32_t bmap_blk = sb_->bmap_start + byte_idx / sb_->block_size;
  uint32_t offset = byte_idx % sb_->block_size;
  
  std::vector<uint8_t> buf(sb_->block_size);
  if (!dev_->ReadBlock(bmap_blk, buf.data())) return false;
  
  buf[offset] &= ~(1 << bit_idx);
  
  return dev_->WriteBlock(bmap_blk, buf.data());
}

bool Vfs::ReadInodeBlock(const Inode& inode, uint32_t block_idx, std::vector<uint8_t>* out) {
  out->resize(sb_->block_size);
  
  if (block_idx < kDirectBlocks) {
    if (inode.direct[block_idx] == 0) {
      std::fill(out->begin(), out->end(), 0);
      return true;
    }
    return dev_->ReadBlock(inode.direct[block_idx], out->data());
  }
  
  // Indirect block
  block_idx -= kDirectBlocks;
  uint32_t ptrs_per_block = sb_->block_size / 4;
  
  if (block_idx < ptrs_per_block) {
    if (inode.indirect1 == 0) {
      std::fill(out->begin(), out->end(), 0);
      return true;
    }
    
    std::vector<uint8_t> ind_buf(sb_->block_size);
    if (!dev_->ReadBlock(inode.indirect1, ind_buf.data())) return false;
    
    uint32_t* ptrs = reinterpret_cast<uint32_t*>(ind_buf.data());
    if (ptrs[block_idx] == 0) {
      std::fill(out->begin(), out->end(), 0);
      return true;
    }
    
    return dev_->ReadBlock(ptrs[block_idx], out->data());
  }
  
  return false; // beyond indirect1
}

bool Vfs::WriteInodeBlock(Inode* inode, uint32_t block_idx, const std::vector<uint8_t>& data) {
  if (data.size() != sb_->block_size) return false;
  
  if (block_idx < kDirectBlocks) {
    if (inode->direct[block_idx] == 0) {
      uint32_t blk = AllocBlock();
      if (blk == UINT32_MAX) return false;
      inode->direct[block_idx] = blk;
    }
    return dev_->WriteBlock(inode->direct[block_idx], data.data());
  }
  
  // Indirect block
  block_idx -= kDirectBlocks;
  uint32_t ptrs_per_block = sb_->block_size / 4;
  
  if (block_idx < ptrs_per_block) {
    if (inode->indirect1 == 0) {
      uint32_t blk = AllocBlock();
      if (blk == UINT32_MAX) return false;
      inode->indirect1 = blk;
    }
    
    std::vector<uint8_t> ind_buf(sb_->block_size);
    if (!dev_->ReadBlock(inode->indirect1, ind_buf.data())) return false;
    
    uint32_t* ptrs = reinterpret_cast<uint32_t*>(ind_buf.data());
    if (ptrs[block_idx] == 0) {
      uint32_t blk = AllocBlock();
      if (blk == UINT32_MAX) return false;
      ptrs[block_idx] = blk;
      if (!dev_->WriteBlock(inode->indirect1, ind_buf.data())) return false;
    }
    
    return dev_->WriteBlock(ptrs[block_idx], data.data());
  }
  
  return false;
}

int Vfs::Lookup(const std::string& path, uint32_t* out_ino, uint32_t* out_parent_ino, std::string* out_name) {
  if (path.empty() || path[0] != '/') return VFS_EINVAL;
  
  if (path == "/") {
    if (out_ino) *out_ino = sb_->root_inode;
    return 0;
  }
  
  // Split path
  std::vector<std::string> parts;
  size_t start = 1;
  while (start < path.size()) {
    size_t end = path.find('/', start);
    if (end == std::string::npos) end = path.size();
    if (end > start) {
      parts.push_back(path.substr(start, end - start));
    }
    start = end + 1;
  }
  
  if (parts.empty()) return VFS_EINVAL;
  
  uint32_t current_ino = sb_->root_inode;
  uint32_t parent_ino = sb_->root_inode;
  
  for (size_t i = 0; i < parts.size(); ++i) {
    const std::string& name = parts[i];
    
    Inode dir_inode;
    if (!ReadInode(current_ino, &dir_inode)) return VFS_EIO;
    if (dir_inode.mode != 2) return VFS_ENOTDIR;
    
    bool found = false;
    uint32_t num_entries = dir_inode.size / kDirEntrySize;
    uint32_t entries_per_block = sb_->block_size / kDirEntrySize;
    
    for (uint32_t j = 0; j < num_entries; ++j) {
      uint32_t block_idx = j / entries_per_block;
      uint32_t entry_idx = j % entries_per_block;
      
      std::vector<uint8_t> buf;
      if (!ReadInodeBlock(dir_inode, block_idx, &buf)) return VFS_EIO;
      
      DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
      if (entries[entry_idx].inode != 0 && strcmp(entries[entry_idx].name, name.c_str()) == 0) {
        parent_ino = current_ino;
        current_ino = entries[entry_idx].inode;
        found = true;
        break;
      }
    }
    
    if (!found) {
      // Last component not found - set parent to current directory
      if (i == parts.size() - 1) {
        if (out_parent_ino) *out_parent_ino = current_ino;
        if (out_name) *out_name = name;
      }
      return VFS_ENOENT;
    }
  }
  
  if (out_ino) *out_ino = current_ino;
  if (out_parent_ino) *out_parent_ino = parent_ino;
  if (out_name) *out_name = parts.back();
  
  return 0;
}

int Vfs::AddDirEntry(uint32_t dir_ino, const std::string& name, uint32_t child_ino) {
  if (name.empty() || name.size() >= 60 || name.find('/') != std::string::npos) {
    return VFS_EINVAL;
  }
  
  Inode dir_inode;
  if (!ReadInode(dir_ino, &dir_inode)) return VFS_EIO;
  if (dir_inode.mode != 2) return VFS_ENOTDIR;
  
  uint32_t num_entries = dir_inode.size / kDirEntrySize;
  uint32_t entries_per_block = sb_->block_size / kDirEntrySize;
  
  // Check if name already exists and find empty slot
  int32_t empty_slot = -1;
  for (uint32_t i = 0; i < num_entries; ++i) {
    uint32_t block_idx = i / entries_per_block;
    uint32_t entry_idx = i % entries_per_block;
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(dir_inode, block_idx, &buf)) return VFS_EIO;
    
    DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
    if (entries[entry_idx].inode == 0) {
      if (empty_slot == -1) empty_slot = i;
    } else if (strcmp(entries[entry_idx].name, name.c_str()) == 0) {
      return VFS_EEXIST;
    }
  }
  
  // Use empty slot or append
  uint32_t slot = (empty_slot >= 0) ? empty_slot : num_entries;
  uint32_t block_idx = slot / entries_per_block;
  uint32_t entry_idx = slot % entries_per_block;
  
  std::vector<uint8_t> buf;
  if (!ReadInodeBlock(dir_inode, block_idx, &buf)) return VFS_EIO;
  
  DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
  entries[entry_idx].inode = child_ino;
  strncpy(entries[entry_idx].name, name.c_str(), 59);
  entries[entry_idx].name[59] = '\0';
  
  if (!WriteInodeBlock(&dir_inode, block_idx, buf)) return VFS_ENOSPC;
  
  if (empty_slot == -1) {
    dir_inode.size += kDirEntrySize;
  }
  
  if (!WriteInode(dir_ino, dir_inode)) return VFS_EIO;
  
  return 0;
}

int Vfs::RemoveDirEntry(uint32_t dir_ino, const std::string& name) {
  Inode dir_inode;
  if (!ReadInode(dir_ino, &dir_inode)) return VFS_EIO;
  if (dir_inode.mode != 2) return VFS_ENOTDIR;
  
  uint32_t num_entries = dir_inode.size / kDirEntrySize;
  uint32_t entries_per_block = sb_->block_size / kDirEntrySize;
  
  for (uint32_t i = 0; i < num_entries; ++i) {
    uint32_t block_idx = i / entries_per_block;
    uint32_t entry_idx = i % entries_per_block;
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(dir_inode, block_idx, &buf)) return VFS_EIO;
    
    DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
    if (entries[entry_idx].inode != 0 && strcmp(entries[entry_idx].name, name.c_str()) == 0) {
      entries[entry_idx].inode = 0;
      memset(entries[entry_idx].name, 0, 60);
      return WriteInodeBlock(&dir_inode, block_idx, buf) ? 0 : EIO;
    }
  }
  
  return VFS_ENOENT;
}

bool Vfs::IsDirEmpty(uint32_t dir_ino) {
  Inode dir_inode;
  if (!ReadInode(dir_ino, &dir_inode)) return false;
  if (dir_inode.mode != 2) return false;
  
  uint32_t num_entries = dir_inode.size / kDirEntrySize;
  uint32_t entries_per_block = sb_->block_size / kDirEntrySize;
  
  for (uint32_t i = 0; i < num_entries; ++i) {
    uint32_t block_idx = i / entries_per_block;
    uint32_t entry_idx = i % entries_per_block;
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(dir_inode, block_idx, &buf)) return false;
    
    DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
    if (entries[entry_idx].inode != 0) return false;
  }
  
  return true;
}

// ====== Public API ======

int Vfs::Mkdir(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  
  uint32_t parent_ino, ino;
  std::string name;
  int ret = Lookup(path, &ino, &parent_ino, &name);
  
  if (ret == 0) return VFS_EEXIST;
  if (ret != VFS_ENOENT) return ret;
  
  // Verify parent is directory
  Inode parent;
  if (!ReadInode(parent_ino, &parent)) return VFS_EIO;
  if (parent.mode != 2) return VFS_ENOTDIR;
  
  // Allocate inode
  uint32_t new_ino = AllocInode();
  if (new_ino == UINT32_MAX) return VFS_ENOSPC;
  
  // Create directory inode
  Inode new_dir{};
  new_dir.mode = 2;
  new_dir.links = 1;
  new_dir.size = 0;
  std::memset(new_dir.direct, 0, sizeof(new_dir.direct));
  new_dir.indirect1 = 0;
  new_dir.indirect2 = 0;
  
  if (!WriteInode(new_ino, new_dir)) {
    FreeInode(new_ino);
    return VFS_EIO;
  }
  
  // Add to parent
  ret = AddDirEntry(parent_ino, name, new_ino);
  if (ret != 0) {
    FreeInode(new_ino);
    return ret;
  }
  
  return 0;
}

int Vfs::Rmdir(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  
  if (path == "/") return VFS_EINVAL;
  
  uint32_t ino, parent_ino;
  std::string name;
  int ret = Lookup(path, &ino, &parent_ino, &name);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  if (inode.mode != 2) return VFS_ENOTDIR;
  if (!IsDirEmpty(ino)) return VFS_ENOTEMPTY;
  
  ret = RemoveDirEntry(parent_ino, name);
  if (ret != 0) return ret;
  
  // Free blocks
  for (uint32_t i = 0; i < kDirectBlocks; ++i) {
    if (inode.direct[i] != 0) FreeBlock(inode.direct[i]);
  }
  if (inode.indirect1 != 0) {
    std::vector<uint8_t> ind_buf(sb_->block_size);
    if (dev_->ReadBlock(inode.indirect1, ind_buf.data())) {
      uint32_t* ptrs = reinterpret_cast<uint32_t*>(ind_buf.data());
      for (uint32_t i = 0; i < sb_->block_size / 4; ++i) {
        if (ptrs[i] != 0) FreeBlock(ptrs[i]);
      }
    }
    FreeBlock(inode.indirect1);
  }
  
  FreeInode(ino);
  return 0;
}

int Vfs::ListDir(const std::string& path, std::vector<std::string>* out_names) {
  std::lock_guard<std::mutex> lk(mu_);
  
  if (!out_names) return VFS_EINVAL;
  out_names->clear();
  
  uint32_t ino;
  int ret = Lookup(path, &ino);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  if (inode.mode != 2) return VFS_ENOTDIR;
  
  uint32_t num_entries = inode.size / kDirEntrySize;
  uint32_t entries_per_block = sb_->block_size / kDirEntrySize;
  
  for (uint32_t i = 0; i < num_entries; ++i) {
    uint32_t block_idx = i / entries_per_block;
    uint32_t entry_idx = i % entries_per_block;
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(inode, block_idx, &buf)) return VFS_EIO;
    
    DirEntry* entries = reinterpret_cast<DirEntry*>(buf.data());
    if (entries[entry_idx].inode != 0) {
      out_names->push_back(entries[entry_idx].name);
    }
  }
  
  return 0;
}

int Vfs::CreateFile(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  
  uint32_t parent_ino, ino;
  std::string name;
  int ret = Lookup(path, &ino, &parent_ino, &name);
  
  if (ret == 0) return VFS_EEXIST;
  if (ret != VFS_ENOENT) return ret;
  
  // Verify parent is directory
  Inode parent;
  if (!ReadInode(parent_ino, &parent)) return VFS_EIO;
  if (parent.mode != 2) return VFS_ENOTDIR;
  
  // Allocate inode
  uint32_t new_ino = AllocInode();
  if (new_ino == UINT32_MAX) return VFS_ENOSPC;
  
  // Create file inode
  Inode new_file{};
  new_file.mode = 1;
  new_file.links = 1;
  new_file.size = 0;
  std::memset(new_file.direct, 0, sizeof(new_file.direct));
  new_file.indirect1 = 0;
  new_file.indirect2 = 0;
  
  if (!WriteInode(new_ino, new_file)) {
    FreeInode(new_ino);
    return VFS_EIO;
  }
  
  // Add to parent
  ret = AddDirEntry(parent_ino, name, new_ino);
  if (ret != 0) {
    FreeInode(new_ino);
    return ret;
  }
  
  return 0;
}

int Vfs::Unlink(const std::string& path) {
  std::lock_guard<std::mutex> lk(mu_);
  
  if (path == "/") return VFS_EINVAL;
  
  uint32_t ino, parent_ino;
  std::string name;
  int ret = Lookup(path, &ino, &parent_ino, &name);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  
  // Allow unlinking files or empty directories
  if (inode.mode == 2 && !IsDirEmpty(ino)) return VFS_ENOTEMPTY;
  
  ret = RemoveDirEntry(parent_ino, name);
  if (ret != 0) return ret;
  
  // Free blocks
  for (uint32_t i = 0; i < kDirectBlocks; ++i) {
    if (inode.direct[i] != 0) FreeBlock(inode.direct[i]);
  }
  if (inode.indirect1 != 0) {
    std::vector<uint8_t> ind_buf(sb_->block_size);
    if (dev_->ReadBlock(inode.indirect1, ind_buf.data())) {
      uint32_t* ptrs = reinterpret_cast<uint32_t*>(ind_buf.data());
      for (uint32_t i = 0; i < sb_->block_size / 4; ++i) {
        if (ptrs[i] != 0) FreeBlock(ptrs[i]);
      }
    }
    FreeBlock(inode.indirect1);
  }
  
  FreeInode(ino);
  return 0;
}

int Vfs::ReadFile(const std::string& path, uint64_t off, uint32_t n, std::string* out) {
  std::lock_guard<std::mutex> lk(mu_);
  
  if (!out) return VFS_EINVAL;
  out->clear();
  
  uint32_t ino;
  int ret = Lookup(path, &ino);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  if (inode.mode != 1) return VFS_EISDIR;
  
  if (off >= inode.size) return 0;
  
  uint64_t to_read = std::min(static_cast<uint64_t>(n), inode.size - off);
  out->resize(to_read);
  
  uint64_t read_pos = 0;
  while (read_pos < to_read) {
    uint64_t file_pos = off + read_pos;
    uint32_t block_idx = file_pos / sb_->block_size;
    uint32_t block_off = file_pos % sb_->block_size;
    uint32_t chunk = std::min(sb_->block_size - block_off, static_cast<uint32_t>(to_read - read_pos));
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(inode, block_idx, &buf)) return VFS_EIO;
    
    std::memcpy(&(*out)[read_pos], buf.data() + block_off, chunk);
    read_pos += chunk;
  }
  
  return 0;
}

int Vfs::WriteFile(const std::string& path, uint64_t off, const std::string& data) {
  std::lock_guard<std::mutex> lk(mu_);
  
  uint32_t ino;
  int ret = Lookup(path, &ino);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  if (inode.mode != 1) return VFS_EISDIR;
  
  uint64_t write_pos = 0;
  while (write_pos < data.size()) {
    uint64_t file_pos = off + write_pos;
    uint32_t block_idx = file_pos / sb_->block_size;
    uint32_t block_off = file_pos % sb_->block_size;
    uint32_t chunk = std::min(sb_->block_size - block_off, static_cast<uint32_t>(data.size() - write_pos));
    
    std::vector<uint8_t> buf;
    if (!ReadInodeBlock(inode, block_idx, &buf)) return VFS_EIO;
    
    std::memcpy(buf.data() + block_off, data.data() + write_pos, chunk);
    
    if (!WriteInodeBlock(&inode, block_idx, buf)) return VFS_ENOSPC;
    
    write_pos += chunk;
  }
  
  if (off + data.size() > inode.size) {
    inode.size = off + data.size();
  }
  
  if (!WriteInode(ino, inode)) return VFS_EIO;
  
  return 0;
}

int Vfs::Truncate(const std::string& path, uint64_t new_size) {
  std::lock_guard<std::mutex> lk(mu_);
  
  uint32_t ino;
  int ret = Lookup(path, &ino);
  if (ret != 0) return ret;
  
  Inode inode;
  if (!ReadInode(ino, &inode)) return VFS_EIO;
  if (inode.mode != 1) return VFS_EISDIR;
  
  if (new_size > inode.size) {
    // Extend with zeros
    std::string zeros(new_size - inode.size, '\0');
    return WriteFile(path, inode.size, zeros);
  } else if (new_size < inode.size) {
    // Shrink - free trailing blocks if needed
    inode.size = new_size;
    if (!WriteInode(ino, inode)) return VFS_EIO;
  }
  
  return 0;
}
