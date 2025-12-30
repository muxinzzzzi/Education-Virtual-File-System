#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
#include <mutex>

#include "block_device.h"

// VFS Error codes (negative values, VFS_ prefix to avoid conflict with system errno)
constexpr int VFS_EPERM     = -1;
constexpr int VFS_ENOENT    = -2;
constexpr int VFS_EEXIST    = -3;
constexpr int VFS_ENOTDIR   = -4;
constexpr int VFS_EISDIR    = -5;
constexpr int VFS_ENOTEMPTY = -6;
constexpr int VFS_EINVAL    = -7;
constexpr int VFS_ENOSPC    = -8;
constexpr int VFS_EIO       = -9;

class Vfs {
public:
  ~Vfs();
  
  // Format
  static bool Mkfs(std::shared_ptr<IBlockDevice> dev);

  // Mount existing filesystem
  static std::unique_ptr<Vfs> Mount(std::shared_ptr<IBlockDevice> dev);

  // Directory ops
  int Mkdir(const std::string& path);
  int Rmdir(const std::string& path);
  int ListDir(const std::string& path, std::vector<std::string>* out_names);

  // File ops
  int CreateFile(const std::string& path);
  int Unlink(const std::string& path);

  // Read/write by offset
  int ReadFile(const std::string& path, uint64_t off, uint32_t n, std::string* out);
  int WriteFile(const std::string& path, uint64_t off, const std::string& data);

  // Optional
  int Truncate(const std::string& path, uint64_t new_size);

private:
  explicit Vfs(std::shared_ptr<IBlockDevice> dev);
  
  // Internal structures
  struct SuperBlock;
  struct Inode;
  struct DirEntry;
  
  // Internal helpers
  bool LoadSuperBlock();
  bool ReadInode(uint32_t ino, Inode* out);
  bool WriteInode(uint32_t ino, const Inode& in);
  uint32_t AllocInode();
  bool FreeInode(uint32_t ino);
  uint32_t AllocBlock();
  bool FreeBlock(uint32_t block_id);
  bool ReadInodeBlock(const Inode& inode, uint32_t block_idx, std::vector<uint8_t>* out);
  bool WriteInodeBlock(Inode* inode, uint32_t block_idx, const std::vector<uint8_t>& data);
  int Lookup(const std::string& path, uint32_t* out_ino, uint32_t* out_parent_ino = nullptr, std::string* out_name = nullptr);
  int AddDirEntry(uint32_t dir_ino, const std::string& name, uint32_t child_ino);
  int RemoveDirEntry(uint32_t dir_ino, const std::string& name);
  bool IsDirEmpty(uint32_t dir_ino);

  std::shared_ptr<IBlockDevice> dev_;
  std::mutex mu_;
  
  // Cached superblock
  struct SuperBlock* sb_ = nullptr;
};
