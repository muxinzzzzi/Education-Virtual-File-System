#include "filesystem/vfs.h"
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
namespace vfs {

int VirtualFileSystem::open(const std::string &path, int flags) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  int32_t inode_num = resolve_path(path);
  if (inode_num < 0) {
    return -1; // File not found
  }

  Inode inode;
  if (!read_inode(inode_num, inode)) {
    return -1;
  }

  if ((inode.mode & S_IFMT) != S_IFREG) {
    return -2; // Not a regular file
  }

  // Handle truncation if requested (and writing is allowed)
  if ((flags & O_TRUNC) && ((flags & O_WRONLY) || (flags & O_RDWR))) {
    // Free direct blocks
    for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {
      if (inode.direct_blocks[i] != 0) {
        free_block(inode.direct_blocks[i]);
        inode.direct_blocks[i] = 0;
      }
    }

    // Free indirect block
    if (inode.indirect_block != 0) {
      std::vector<char> indirect_data;
      if (read_block(inode.indirect_block, indirect_data)) {
        uint32_t *ptrs = reinterpret_cast<uint32_t *>(indirect_data.data());
        uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);

        for (uint32_t i = 0; i < ptrs_per_block; ++i) {
          if (ptrs[i] != 0) {
            free_block(ptrs[i]);
          }
        }
      }
      free_block(inode.indirect_block);
      inode.indirect_block = 0;
    }

    // Double indirect would go here if/when supported

    inode.size = 0;
    inode.blocks_count = 0;
    inode.mtime = std::time(nullptr);
    write_inode(inode_num, inode);
  }

  int fd = allocate_fd();
  FileDescriptor &file_desc = fd_table_[fd];
  file_desc.inode_num = inode_num;
  file_desc.offset = 0;
  file_desc.flags = flags;
  file_desc.is_open = true;

  // Update access time
  inode.atime = std::time(nullptr);
  write_inode(inode_num, inode);

  return fd;
}

int VirtualFileSystem::close(int fd) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end() || !it->second.is_open) {
    return -1; // Invalid file descriptor
  }

  free_fd(fd);
  return 0;
}

ssize_t VirtualFileSystem::read(int fd, void *buffer, size_t count) {
  std::shared_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end() || !it->second.is_open) {
    return -1;
  }

  FileDescriptor &file_desc = it->second;

  Inode inode;
  if (!read_inode(file_desc.inode_num, inode)) {
    return -1;
  }

  // Calculate how much to read
  if (file_desc.offset >= inode.size) {
    return 0; // EOF
  }

  size_t to_read =
      std::min(count, static_cast<size_t>(inode.size - file_desc.offset));
  size_t bytes_read = 0;
  char *buf = static_cast<char *>(buffer);

  while (bytes_read < to_read) {
    uint64_t current_pos = file_desc.offset + bytes_read;
    uint32_t block_index = current_pos / BLOCK_SIZE;
    uint32_t offset_in_block = current_pos % BLOCK_SIZE;

    // Get physical block number
    uint32_t physical_block = 0;
    if (block_index < DIRECT_BLOCKS) {
      physical_block = inode.direct_blocks[block_index];
    } else {
      // Handle single indirect blocks
      uint32_t indirect_index = block_index - DIRECT_BLOCKS;
      uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);

      if (indirect_index < ptrs_per_block) {
        if (inode.indirect_block != 0) {
          std::vector<char> indirect_data;
          if (const_cast<VirtualFileSystem *>(this)->read_block(
                  inode.indirect_block, indirect_data)) {
            uint32_t *ptrs = reinterpret_cast<uint32_t *>(indirect_data.data());
            physical_block = ptrs[indirect_index];
          }
        }
      } else {
        // Handle double indirect blocks if needed, but for now we stop here
        break;
      }
    }

    if (physical_block == 0) {
      break; // Sparse file or reaching end of allocated blocks
    }

    // Read from block
    std::vector<char> block_data;
    if (!const_cast<VirtualFileSystem *>(this)->read_block(physical_block,
                                                           block_data)) {
      break;
    }

    size_t copy_size =
        std::min(to_read - bytes_read,
                 static_cast<size_t>(BLOCK_SIZE - offset_in_block));
    std::memcpy(buf + bytes_read, block_data.data() + offset_in_block,
                copy_size);
    bytes_read += copy_size;
  }

  // Update offset and access time
  const_cast<FileDescriptor &>(file_desc).offset += bytes_read;
  inode.atime = std::time(nullptr);
  const_cast<VirtualFileSystem *>(this)->write_inode(file_desc.inode_num,
                                                     inode);

  return bytes_read;
}

ssize_t VirtualFileSystem::write(int fd, const void *buffer, size_t count) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end() || !it->second.is_open) {
    return -1;
  }

  FileDescriptor &file_desc = it->second;

  Inode inode;
  if (!read_inode(file_desc.inode_num, inode)) {
    return -1;
  }

  size_t bytes_written = 0;
  const char *buf = static_cast<const char *>(buffer);

  while (bytes_written < count) {
    uint64_t current_pos = file_desc.offset + bytes_written;
    uint32_t block_index = current_pos / BLOCK_SIZE;
    uint32_t offset_in_block = current_pos % BLOCK_SIZE;

    // Get or allocate physical block
    uint32_t physical_block = 0;
    if (block_index < DIRECT_BLOCKS) {
      if (inode.direct_blocks[block_index] == 0) {
        // Allocate new block
        physical_block = allocate_block();
        if (physical_block == static_cast<uint32_t>(-1)) {
          break; // No free blocks
        }
        inode.direct_blocks[block_index] = physical_block;
        inode.blocks_count++;

        // Zero out new block
        std::vector<char> zero_block(BLOCK_SIZE, 0);
        write_block(physical_block, zero_block);
      } else {
        physical_block = inode.direct_blocks[block_index];
      }
    } else {
      // Handle single indirect blocks
      uint32_t indirect_index = block_index - DIRECT_BLOCKS;
      uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);

      if (indirect_index < ptrs_per_block) {
        if (inode.indirect_block == 0) {
          // Allocate indirect block
          uint32_t indirect_block_num = allocate_block();
          if (indirect_block_num == static_cast<uint32_t>(-1)) {
            break;
          }
          inode.indirect_block = indirect_block_num;
          // Zero out indirect block
          std::vector<char> zero_block(BLOCK_SIZE, 0);
          write_block(indirect_block_num, zero_block);
        }

        // Read indirect block
        std::vector<char> indirect_data;
        if (!read_block(inode.indirect_block, indirect_data)) {
          break;
        }

        uint32_t *ptrs = reinterpret_cast<uint32_t *>(indirect_data.data());
        if (ptrs[indirect_index] == 0) {
          // Allocate new data block
          uint32_t data_block_num = allocate_block();
          if (data_block_num == static_cast<uint32_t>(-1)) {
            break;
          }
          ptrs[indirect_index] = data_block_num;
          inode.blocks_count++;

          // Write updated indirect block
          if (!write_block(inode.indirect_block, indirect_data)) {
            break;
          }

          // Zero out new data block
          std::vector<char> zero_block(BLOCK_SIZE, 0);
          write_block(data_block_num, zero_block);
          physical_block = data_block_num;
        } else {
          physical_block = ptrs[indirect_index];
        }
      } else {
        // Double indirect omitted for now
        break;
      }
    }

    if (physical_block == 0) {
      break;
    }

    // Read existing block data
    std::vector<char> block_data;
    if (!read_block(physical_block, block_data)) {
      break;
    }

    // Modify block
    size_t copy_size =
        std::min(count - bytes_written,
                 static_cast<size_t>(BLOCK_SIZE - offset_in_block));
    std::memcpy(block_data.data() + offset_in_block, buf + bytes_written,
                copy_size);

    // Write back
    if (!write_block(physical_block, block_data)) {
      break;
    }

    bytes_written += copy_size;
  }

  // Update file size and times
  file_desc.offset += bytes_written;
  if (file_desc.offset > inode.size) {
    inode.size = file_desc.offset;
  }
  inode.mtime = inode.atime = std::time(nullptr);
  write_inode(file_desc.inode_num, inode);

  return bytes_written;
}

off_t VirtualFileSystem::seek(int fd, off_t offset, int whence) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  auto it = fd_table_.find(fd);
  if (it == fd_table_.end() || !it->second.is_open) {
    return -1;
  }

  FileDescriptor &file_desc = it->second;

  Inode inode;
  if (!read_inode(file_desc.inode_num, inode)) {
    return -1;
  }

  off_t new_offset;
  switch (whence) {
  case SEEK_SET:
    new_offset = offset;
    break;
  case SEEK_CUR:
    new_offset = file_desc.offset + offset;
    break;
  case SEEK_END:
    new_offset = inode.size + offset;
    break;
  default:
    return -1;
  }

  if (new_offset < 0) {
    return -1;
  }

  file_desc.offset = new_offset;
  return new_offset;
}

int VirtualFileSystem::delete_file(const std::string &path) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  std::string name;
  int32_t parent_inode = resolve_path_parent(path, name);
  if (parent_inode < 0) {
    return -1;
  }

  int32_t inode_num = find_dir_entry(parent_inode, name);
  if (inode_num < 0) {
    return -1; // File not found
  }

  Inode inode;
  if (!read_inode(inode_num, inode)) {
    return -1;
  }

  if ((inode.mode & S_IFMT) != S_IFREG) {
    return -2; // Not a regular file
  }

  // Free direct blocks
  for (uint32_t i = 0; i < DIRECT_BLOCKS; ++i) {
    if (inode.direct_blocks[i] != 0) {
      free_block(inode.direct_blocks[i]);
    }
  }

  // Free indirect block
  if (inode.indirect_block != 0) {
    std::vector<char> indirect_data;
    if (read_block(inode.indirect_block, indirect_data)) {
      uint32_t *ptrs = reinterpret_cast<uint32_t *>(indirect_data.data());
      uint32_t ptrs_per_block = BLOCK_SIZE / sizeof(uint32_t);

      for (uint32_t i = 0; i < ptrs_per_block; ++i) {
        if (ptrs[i] != 0) {
          free_block(ptrs[i]);
        }
      }
    }
    free_block(inode.indirect_block);
  }

  // Free inode
  free_inode(inode_num);

  // Remove directory entry
  remove_dir_entry(parent_inode, name);

  return 0;
}

int VirtualFileSystem::rmdir(const std::string &path) {
  std::unique_lock<std::shared_mutex> lock(fs_mutex_);

  if (!mounted_) {
    return -1;
  }

  std::string name;
  int32_t parent_inode = resolve_path_parent(path, name);
  if (parent_inode < 0) {
    return -1;
  }

  int32_t inode_num = find_dir_entry(parent_inode, name);
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

  // Check if directory is empty
  std::vector<DirEntry> entries;
  int result = const_cast<VirtualFileSystem *>(this)->readdir(path, entries);
  if (result != 0 || !entries.empty()) {
    return -3; // Directory not empty or error
  }

  // Free directory blocks
  for (uint32_t i = 0; i < DIRECT_BLOCKS && inode.direct_blocks[i] != 0; ++i) {
    free_block(inode.direct_blocks[i]);
  }

  // Free inode
  free_inode(inode_num);

  // Remove directory entry
  remove_dir_entry(parent_inode, name);

  return 0;
}

} // namespace vfs
