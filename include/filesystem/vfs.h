#ifndef VFS_H
#define VFS_H

#include "bitmap.h"
#include "lru_cache.h"
#include "vfs_types.h"
#include <fstream>
#include <memory>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <vector>

namespace vfs {

/**
 * @brief Virtual File System implementation
 * Provides a complete file system with directory structure,
 * file I/O, caching, and backup capabilities
 */
class VirtualFileSystem {
public:
  VirtualFileSystem();
  ~VirtualFileSystem();

  // ===== Initialization =====

  /**
   * @brief Format a new file system
   * @param image_path Path to the image file
   * @param size_mb Total size in megabytes
   * @param cache_capacity Number of blocks to cache (default 256)
   * @return true if successful
   */
  bool format(const std::string &image_path, uint32_t size_mb,
              size_t cache_capacity = 256);

  /**
   * @brief Mount an existing file system
   * @param image_path Path to the image file
   * @param cache_capacity Number of blocks to cache (default 256)
   * @return true if successful
   */
  bool mount(const std::string &image_path, size_t cache_capacity = 256);

  /**
   * @brief Unmount the file system
   */
  void unmount();

  /**
   * @brief Check if file system is mounted
   */
  bool is_mounted() const { return mounted_; }

  // ===== File Operations =====

  /**
   * @brief Create a new file
   * @param path File path
   * @param mode File permissions
   * @return 0 on success, negative error code on failure
   */
  int create_file(const std::string &path, uint32_t mode = S_IRUSR | S_IWUSR);

  /**
   * @brief Delete a file
   * @param path File path
   * @return 0 on success, negative error code on failure
   */
  int delete_file(const std::string &path);

  /**
   * @brief Open a file
   * @param path File path
   * @param flags Open flags (O_RDONLY, O_WRONLY, O_RDWR, etc.)
   * @return file descriptor (>= 0) on success, negative error code on failure
   */
  int open(const std::string &path, int flags);

  /**
   * @brief Close a file
   * @param fd File descriptor
   * @return 0 on success, negative error code on failure
   */
  int close(int fd);

  /**
   * @brief Read from a file
   * @param fd File descriptor
   * @param buffer Output buffer
   * @param count Number of bytes to read
   * @return number of bytes read, or negative error code
   */
  ssize_t read(int fd, void *buffer, size_t count);

  /**
   * @brief Write to a file
   * @param fd File descriptor
   * @param buffer Input buffer
   * @param count Number of bytes to write
   * @return number of bytes written, or negative error code
   */
  ssize_t write(int fd, const void *buffer, size_t count);

  /**
   * @brief Seek to a position in a file
   * @param fd File descriptor
   * @param offset Offset
   * @param whence SEEK_SET, SEEK_CUR, or SEEK_END
   * @return new offset, or negative error code
   */
  off_t seek(int fd, off_t offset, int whence);

  // ===== Directory Operations =====

  /**
   * @brief Create a directory
   * @param path Directory path
   * @param mode Directory permissions
   * @return 0 on success, negative error code on failure
   */
  int mkdir(const std::string &path, uint32_t mode = S_IRWXU);

  /**
   * @brief Remove a directory
   * @param path Directory path
   * @return 0 on success, negative error code on failure
   */
  int rmdir(const std::string &path);

  /**
   * @brief List directory contents
   * @param path Directory path
   * @param entries Output vector of directory entries
   * @return 0 on success, negative error code on failure
   */
  int readdir(const std::string &path, std::vector<DirEntry> &entries);

  /**
   * @brief Check if path exists
   */
  bool exists(const std::string &path);

  /**
   * @brief Check if path is a directory
   */
  bool is_directory(const std::string &path);

  // ===== Backup Operations =====

  /**
   * @brief Create a backup snapshot
   * @param backup_name Backup name
   * @return true if successful
   */
  bool create_backup(const std::string &backup_name);

  /**
   * @brief List all backups
   * @return vector of backup names
   */
  std::vector<std::string> list_backups();

  /**
   * @brief Restore from a backup
   * @param backup_name Backup name
   * @return true if successful
   */
  bool restore_backup(const std::string &backup_name);

  /**
   * @brief Create a copy-on-write snapshot
   */
  bool create_snapshot(const std::string &name);

  /**
   * @brief List copy-on-write snapshots
   */
  std::vector<std::string> list_snapshots();

  /**
   * @brief Restore a snapshot and remove it
   */
  bool restore_snapshot(const std::string &name);

  // ===== Statistics =====

  /**
   * @brief Get file system statistics
   */
  FileSystemStats get_fs_stats() const;

  /**
   * @brief Get cache statistics
   */
  CacheStats get_cache_stats() const;

  struct JournalStats {
    uint64_t replayed{0};
    uint64_t pending{0};
    bool recovered{false};
    bool dirty{false};
  };

  /**
   * @brief Get journaling statistics
   */
  JournalStats get_journal_stats() const;

private:
  // File system state
  bool mounted_;
  std::string image_path_;
  std::fstream image_file_;
  std::string journal_path_;
  std::string checksum_path_;

  // Core structures
  Superblock superblock_;
  std::unique_ptr<Bitmap> bitmap_;
  std::unique_ptr<LRUCache> cache_;
  std::vector<uint32_t> block_checksums_;
  JournalStats journal_stats_;

  struct SnapshotMeta {
    std::string name;
    std::string diff_path;
    std::string index_path;
    std::unordered_set<uint32_t> blocks;
  };
  std::map<std::string, SnapshotMeta> snapshots_;

  // File descriptors
  std::unordered_map<int, FileDescriptor> fd_table_;
  int next_fd_;

  // Concurrency control
  mutable std::shared_mutex fs_mutex_;

  // ===== Low-level block operations =====
  bool read_block(uint32_t block_num, std::vector<char> &data);
  bool write_block(uint32_t block_num, const std::vector<char> &data);

  // ===== Inode operations =====
  bool read_inode(uint32_t inode_num, Inode &inode);
  bool write_inode(uint32_t inode_num, const Inode &inode);
  uint32_t allocate_inode();
  bool free_inode(uint32_t inode_num);

  // ===== Block operations =====
  uint32_t allocate_block();
  bool free_block(uint32_t block_num);

  // ===== Path operations =====
  int32_t resolve_path(const std::string &path);
  int32_t resolve_path_parent(const std::string &path, std::string &name);
  std::vector<std::string> split_path(const std::string &path);

  // ===== Directory operations =====
  bool add_dir_entry(uint32_t dir_inode, const std::string &name,
                     uint32_t inode_num, FileType type);
  bool remove_dir_entry(uint32_t dir_inode, const std::string &name);
  int32_t find_dir_entry(uint32_t dir_inode, const std::string &name);

  // ===== Helpers =====
  void init_root_directory();
  int allocate_fd();
  void free_fd(int fd);

  // Journaling helpers
  bool replay_journal();
  bool append_journal_entry(uint32_t block_num,
                            const std::vector<char> &data);
  bool flush_and_clear_journal();

  // Checksums
  uint32_t calc_checksum(const std::vector<char> &data) const;
  void load_checksums();
  void save_checksums();

  // Snapshots (COW)
  void load_snapshots();
  bool snapshot_record_block(uint32_t block_num,
                             const std::vector<char> &original);
};

} // namespace vfs

#endif // VFS_H
