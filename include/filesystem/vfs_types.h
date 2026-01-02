#ifndef VFS_TYPES_H
#define VFS_TYPES_H

#include <cstdint>
#include <ctime>

namespace vfs {

// Constants
constexpr uint32_t MAGIC_NUMBER = 0x52455644; // 'REVD'
constexpr uint32_t BLOCK_SIZE = 4096;
constexpr uint32_t DIRECT_BLOCKS = 12;
constexpr uint32_t MAX_FILENAME = 255;

// File types
enum class FileType : uint8_t {
  UNKNOWN = 0,
  REGULAR = 1,
  DIRECTORY = 2,
  SYMLINK = 3
};

// File modes
constexpr uint32_t S_IFMT = 0170000;
constexpr uint32_t S_IFREG = 0100000;
constexpr uint32_t S_IFDIR = 0040000;
constexpr uint32_t S_IRWXU = 00700;
constexpr uint32_t S_IRUSR = 00400;
constexpr uint32_t S_IWUSR = 00200;
constexpr uint32_t S_IXUSR = 00100;

// Superblock structure
struct Superblock {
  uint32_t magic;             // Magic number: 0x52455643 ('REVC')
  uint32_t version;           // File system version
  uint32_t block_size;        // Block size in bytes
  uint32_t total_blocks;      // Total number of blocks
  uint32_t total_inodes;      // Total number of inodes
  uint32_t free_blocks;       // Number of free blocks
  uint32_t free_inodes;       // Number of free inodes
  uint32_t inode_table_block; // Starting block of inode table
  uint32_t data_block_start;  // Starting block of data area
  uint32_t bitmap_block;      // Starting block of bitmap
  uint64_t created_time;      // Creation timestamp
  uint64_t modified_time;     // Last modification timestamp
  char reserved[256];         // Reserved for future use

  Superblock()
      : magic(MAGIC_NUMBER), version(1), block_size(BLOCK_SIZE),
        total_blocks(0), total_inodes(0), free_blocks(0), free_inodes(0),
        inode_table_block(1), data_block_start(0), bitmap_block(0),
        created_time(0), modified_time(0), reserved{} {}
} __attribute__((packed));

// Inode structure (must be fixed size and aligned)
struct Inode {
  uint32_t inode_num;                    // Inode number
  uint32_t mode;                         // File type and permissions
  uint32_t uid;                          // Owner user ID
  uint32_t gid;                          // Owner group ID
  uint64_t size;                         // File size in bytes
  uint64_t atime;                        // Access time
  uint64_t mtime;                        // Modification time
  uint64_t ctime;                        // Creation time
  uint32_t links_count;                  // Hard link count
  uint32_t blocks_count;                 // Number of blocks used
  uint32_t direct_blocks[DIRECT_BLOCKS]; // Direct block pointers
  uint32_t indirect_block;               // Single indirect block pointer
  uint32_t double_indirect;              // Double indirect block pointer
  char padding[12];                      // Padding to make it 128 bytes

  Inode()
      : inode_num(0), mode(0), uid(0), gid(0), size(0), atime(0), mtime(0),
        ctime(0), links_count(0), blocks_count(0), direct_blocks{},
        indirect_block(0), double_indirect(0), padding{} {}
};

static_assert(sizeof(Inode) == 128, "Inode size must be 128 bytes");

// Directory entry structure (must be fixed size and aligned)
struct DirEntry {
  uint32_t inode_num; // Inode number
  uint16_t rec_len;   // Record length (for future use with variable length)
  uint8_t name_len;   // Name length
  uint8_t file_type;  // File type
  char name[MAX_FILENAME]; // File name (255 bytes)
  char padding[1];         // Pad to 264 bytes (multiple of 8)

  DirEntry()
      : inode_num(0), rec_len(sizeof(DirEntry)), name_len(0), file_type(0),
        name{} {
    padding[0] = 0;
  }
};

static_assert(sizeof(DirEntry) == 264, "DirEntry size must be 264 bytes");

// File descriptor structure
struct FileDescriptor {
  uint32_t inode_num;
  uint64_t offset;
  int flags;
  bool is_open;

  FileDescriptor() : inode_num(0), offset(0), flags(0), is_open(false) {}
};

// Cache statistics
struct CacheStats {
  uint64_t hits;
  uint64_t misses;
  uint64_t evictions;
  uint64_t total_requests;

  double hit_rate() const {
    return total_requests > 0 ? static_cast<double>(hits) / total_requests
                              : 0.0;
  }
};

// File system statistics
struct FileSystemStats {
  uint32_t total_blocks;
  uint32_t free_blocks;
  uint32_t total_inodes;
  uint32_t free_inodes;
  uint64_t total_size;
  uint64_t used_size;

  double usage_percent() const {
    return total_size > 0 ? static_cast<double>(used_size) / total_size * 100.0
                          : 0.0;
  }
};

} // namespace vfs

#endif // VFS_TYPES_H
