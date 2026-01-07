#include "filesystem/vfs.h"
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <sstream>

namespace fs = std::filesystem;

namespace vfs {

// Path operations
std::vector<std::string>
VirtualFileSystem::split_path(const std::string &path) {
  std::vector<std::string> components;
  std::stringstream ss(path);
  std::string component;

  while (std::getline(ss, component, '/')) {
    if (!component.empty() && component != ".") {
      if (component == "..") {
        if (!components.empty()) {
          components.pop_back();
        }
      } else {
        components.push_back(component);
      }
    }
  }

  return components;
}

int32_t VirtualFileSystem::resolve_path(const std::string &path) {
  auto components = split_path(path);
  if (components.empty()) {
    return 1; // Root is inode 1
  }

  uint32_t current_inode = 1; // Start from root (inode 1)

  for (const auto &component : components) {
    int32_t next_inode = find_dir_entry(current_inode, component);
    if (next_inode < 0) {
      return -1; // Path not found
    }
    current_inode = next_inode;
  }

  return current_inode;
}

int32_t VirtualFileSystem::resolve_path_parent(const std::string &path,
                                               std::string &name) {
  auto components = split_path(path);
  if (components.empty()) {
    std::cerr << "[VFS DEBUG] resolve_path_parent: Empty components for path '"
              << path << "', cannot determine parent.\n";
    return -1;
  }

  name = components.back();
  components.pop_back();
  std::cerr << "[VFS DEBUG] resolve_path_parent: Path '" << path
            << "', target name is '" << name << "'.\n";

  if (components.empty()) {
    std::cerr << "[VFS DEBUG] resolve_path_parent: Parent of '" << path
              << "' is root (inode 1).\n";
    return 1; // Parent is root (inode 1)
  }

  // Reconstruct parent path
  uint32_t current_inode = 1; // Start from root (inode 1)
  std::cerr << "[VFS DEBUG] resolve_path_parent: Starting parent path "
               "resolution from inode "
            << current_inode << "\n";
  for (const auto &component : components) {
    int32_t next_inode = find_dir_entry(current_inode, component);
    if (next_inode < 0) {
      std::cerr << "[VFS DEBUG] resolve_path_parent: Component '" << component
                << "' not found in directory inode " << current_inode
                << " during parent resolution.\n";
      return -1;
    }
    current_inode = next_inode;
  }

  return current_inode;
}

// Directory operations
bool VirtualFileSystem::add_dir_entry(uint32_t dir_inode,
                                      const std::string &name,
                                      uint32_t inode_num, FileType type) {
  if (name.length() > MAX_FILENAME) {
    return false;
  }

  Inode inode;
  if (!read_inode(dir_inode, inode)) {
    std::cerr << "[VFS DEBUG] add_dir_entry: Failed to read parent inode "
              << dir_inode << "\n";
    return false;
  }

  if ((inode.mode & S_IFMT) != S_IFDIR) {
    std::cerr << "[VFS DEBUG] add_dir_entry: Parent inode " << dir_inode
              << " is NOT a directory (mode=" << std::oct << inode.mode
              << std::dec << ")\n";
    return false; // Not a directory
  }

  // Create directory entry
  DirEntry entry;
  entry.inode_num = inode_num;
  entry.name_len = name.length();
  entry.file_type = static_cast<uint8_t>(type);
  entry.rec_len = sizeof(DirEntry);
  strncpy(entry.name, name.c_str(), MAX_FILENAME);

  // Allocate block if directory is empty
  if (inode.blocks_count == 0) {
    uint32_t block_num = allocate_block();
    if (block_num == static_cast<uint32_t>(-1)) {
      return false;
    }
    inode.direct_blocks[0] = block_num;
    inode.blocks_count = 1;

    std::vector<char> zero_block(BLOCK_SIZE, 0);
    if (!write_block(block_num, zero_block)) {
      std::cerr << "[VFS DEBUG] add_dir_entry: Failed to write zero block "
                << block_num << "\n";
      return false;
    }
  }

  // Write entry to directory block
  std::vector<char> block_data;
  if (!read_block(inode.direct_blocks[0], block_data)) {
    std::cerr << "[VFS DEBUG] add_dir_entry: read_block failed for block "
              << inode.direct_blocks[0] << "\n";
    return false;
  }

  // Find free space in directory
  size_t offset = 0;
  while (offset + sizeof(DirEntry) <= BLOCK_SIZE) {
    DirEntry *existing =
        reinterpret_cast<DirEntry *>(block_data.data() + offset);
    if (existing->inode_num == 0) {
      // Found free slot
      std::memcpy(block_data.data() + offset, &entry, sizeof(DirEntry));
      inode.size += sizeof(DirEntry);
      inode.mtime = std::time(nullptr);
      if (!write_inode(dir_inode, inode)) {
        std::cerr << "[VFS DEBUG] add_dir_entry: write_inode failed\n";
        return false;
      }
      if (!write_block(inode.direct_blocks[0], block_data)) {
        std::cerr << "[VFS DEBUG] add_dir_entry: write_block failed\n";
        return false;
      }
      return true;
    }
    offset += sizeof(DirEntry);
  }

  std::cerr << "[VFS DEBUG] add_dir_entry: Directory full at inode "
            << dir_inode << "\n";
  return false; // Directory full
}

bool VirtualFileSystem::remove_dir_entry(uint32_t dir_inode,
                                         const std::string &name) {
  Inode inode;
  if (!read_inode(dir_inode, inode)) {
    return false;
  }

  if ((inode.mode & S_IFMT) != S_IFDIR) {
    return false;
  }

  if (inode.blocks_count == 0) {
    return false;
  }

  std::vector<char> block_data;
  if (!read_block(inode.direct_blocks[0], block_data)) {
    return false;
  }

  size_t offset = 0;
  while (offset + sizeof(DirEntry) <= BLOCK_SIZE) {
    DirEntry *entry = reinterpret_cast<DirEntry *>(block_data.data() + offset);
    if (entry->inode_num != 0 &&
        std::string(entry->name, entry->name_len) == name) {
      // Found entry, mark as free
      entry->inode_num = 0;
      inode.size -= sizeof(DirEntry);
      inode.mtime = std::time(nullptr);
      write_inode(dir_inode, inode);
      return write_block(inode.direct_blocks[0], block_data);
    }
    offset += sizeof(DirEntry);
  }

  return false;
}

int32_t VirtualFileSystem::find_dir_entry(uint32_t dir_inode,
                                          const std::string &name) {
  Inode inode;
  if (!read_inode(dir_inode, inode)) {
    return -1;
  }

  if ((inode.mode & S_IFMT) != S_IFDIR) {
    return -1;
  }

  if (inode.blocks_count == 0) {
    return -1;
  }

  std::vector<char> block_data;
  if (!read_block(inode.direct_blocks[0], block_data)) {
    return -1;
  }

  size_t offset = 0;
  while (offset + sizeof(DirEntry) <= BLOCK_SIZE) {
    DirEntry *entry = reinterpret_cast<DirEntry *>(block_data.data() + offset);
    if (entry->inode_num != 0 &&
        std::string(entry->name, entry->name_len) == name) {
      return entry->inode_num;
    }
    offset += sizeof(DirEntry);
  }

  return -1;
}

// File descriptor management
int VirtualFileSystem::allocate_fd() {
  int fd = next_fd_++;
  fd_table_[fd] = FileDescriptor();
  return fd;
}

void VirtualFileSystem::free_fd(int fd) { fd_table_.erase(fd); }

// Public file operations
int VirtualFileSystem::create_file(const std::string &path, uint32_t mode) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  std::string name;
  int32_t parent_inode = resolve_path_parent(path, name);
  if (parent_inode < 0) {
    return -1; // Parent directory not found
  }

  // Check if file already exists
  if (find_dir_entry(parent_inode, name) >= 0) {
    return -2; // File already exists
  }

  // Allocate inode
  uint32_t inode_num = allocate_inode();
  if (inode_num == static_cast<uint32_t>(-1)) {
    return -3; // No free inodes
  }

  // Initialize inode
  Inode inode;
  inode.inode_num = inode_num;
  inode.mode = S_IFREG | (mode & 0777);
  inode.uid = 0;
  inode.gid = 0;
  inode.size = 0;
  inode.atime = inode.mtime = inode.ctime = std::time(nullptr);
  inode.links_count = 1;
  inode.blocks_count = 0;

  if (!write_inode(inode_num, inode)) {
    free_inode(inode_num);
    return -4;
  }

  // Add directory entry
  if (!add_dir_entry(parent_inode, name, inode_num, FileType::REGULAR)) {
    free_inode(inode_num);
    return -5;
  }

  return 0;
}

int VirtualFileSystem::mkdir(const std::string &path, uint32_t mode) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  std::string name;
  int32_t parent_inode = resolve_path_parent(path, name);
  if (parent_inode < 0) {
    return -1;
  }

  if (find_dir_entry(parent_inode, name) >= 0) {
    return -2; // Already exists
  }

  uint32_t inode_num = allocate_inode();
  if (inode_num == static_cast<uint32_t>(-1)) {
    return -3;
  }

  Inode inode;
  inode.inode_num = inode_num;
  inode.mode = S_IFDIR | (mode & 0777);
  inode.uid = 0;
  inode.gid = 0;
  inode.size = 0;
  inode.atime = inode.mtime = inode.ctime = std::time(nullptr);
  inode.links_count = 2;
  inode.blocks_count = 0;

  if (!write_inode(inode_num, inode)) {
    free_inode(inode_num);
    return -4;
  }

  if (!add_dir_entry(parent_inode, name, inode_num, FileType::DIRECTORY)) {
    free_inode(inode_num);
    return -5;
  }

  return 0;
}

bool VirtualFileSystem::exists(const std::string &path) {
  std::shared_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return false;
  }

  return resolve_path(path) >= 0;
}

bool VirtualFileSystem::is_directory(const std::string &path) {
  std::shared_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return false;
  }

  int32_t inode_num = resolve_path(path);
  if (inode_num < 0) {
    return false;
  }

  Inode inode;
  if (!read_inode(inode_num, inode)) {
    return false;
  }

  return (inode.mode & S_IFMT) == S_IFDIR;
}

int VirtualFileSystem::readdir(const std::string &path,
                               std::vector<DirEntry> &entries) {
  std::shared_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  int32_t inode_num = resolve_path(path);
  if (inode_num < 0) {
    return -1;
  }

  Inode inode;
  if (!read_inode(inode_num, inode)) {
    return -1;
  }

  if ((inode.mode & S_IFMT) != S_IFDIR) {
    return -2; // Not a directory
  }

  entries.clear();

  if (inode.blocks_count == 0) {
    return 0; // Empty directory
  }

  std::vector<char> block_data;
  if (!read_block(inode.direct_blocks[0], block_data)) {
    return -1;
  }

  size_t offset = 0;
  while (offset + sizeof(DirEntry) <= BLOCK_SIZE) {
    DirEntry *entry = reinterpret_cast<DirEntry *>(block_data.data() + offset);
    if (entry->inode_num != 0) {
      entries.push_back(*entry);
    }
    offset += sizeof(DirEntry);
  }

  return 0;
}

// Backup operations (simplified implementation)
bool VirtualFileSystem::create_backup(const std::string &backup_name) {
  // create_snapshot 内部会自行获取独占锁并检查 mounted 状态
  return create_snapshot(backup_name);
}

std::vector<std::string> VirtualFileSystem::list_backups() {
  return list_snapshots();
}

bool VirtualFileSystem::restore_backup(const std::string &backup_name) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (mounted_) {
    return false; // Must unmount first
  }

  return restore_snapshot(backup_name);
}

bool VirtualFileSystem::create_snapshot(const std::string &name) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);
  if (!mounted_ || name.empty()) {
    return false;
  }

  std::string diff_path = image_path_ + ".snap." + name + ".diff";
  SnapshotMeta meta;
  meta.name = name;
  meta.diff_path = diff_path;
  meta.index_path = diff_path + ".idx";

  std::ofstream diff(diff_path, std::ios::binary | std::ios::trunc);
  if (!diff) {
    return false;
  }

  snapshots_[name] = std::move(meta);
  return true;
}

std::vector<std::string> VirtualFileSystem::list_snapshots() {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);
  if (snapshots_.empty()) {
    load_snapshots();
  }
  std::vector<std::string> names;
  for (const auto &kv : snapshots_) {
    names.push_back(kv.first);
  }
  return names;
}

bool VirtualFileSystem::restore_snapshot(const std::string &name) {
  if (name.empty()) {
    return false;
  }

  // Ensure latest snapshot list
  load_snapshots();
  auto it = snapshots_.find(name);
  if (it == snapshots_.end()) {
    return false;
  }

  SnapshotMeta meta = it->second;
  snapshots_.erase(it);

  std::ifstream diff(meta.diff_path, std::ios::binary);
  if (!diff) {
    return false;
  }

  std::fstream image(image_path_, std::ios::in | std::ios::out | std::ios::binary);
  if (!image) {
    return false;
  }

  while (diff) {
    uint32_t block_num = 0;
    diff.read(reinterpret_cast<char *>(&block_num), sizeof(block_num));
    if (!diff)
      break;
    std::vector<char> buf(BLOCK_SIZE);
    diff.read(buf.data(), BLOCK_SIZE);
    if (!diff)
      break;
    uint64_t offset = static_cast<uint64_t>(block_num) * BLOCK_SIZE;
    image.seekp(offset);
    image.write(buf.data(), BLOCK_SIZE);
  }
  image.flush();

  try {
    fs::remove(meta.diff_path);
    fs::remove(meta.index_path);
    if (!checksum_path_.empty() && fs::exists(checksum_path_)) {
      fs::remove(checksum_path_);
    }
  } catch (...) {
  }

  return true;
}

} // namespace vfs
