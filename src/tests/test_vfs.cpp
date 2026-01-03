#include "filesystem/vfs.h"
#include <cassert>
#include <fcntl.h>
#include <iostream>
#include <cstring>  
using namespace vfs;

void test_format_and_mount() {
  std::cout << "Testing format and mount...\n";

  VirtualFileSystem vfs;

  // Format a 10MB file system
  bool success = vfs.format("/tmp/test_fs.img", 10, 128);
  assert(success && "Format failed");

  // Check stats
  auto stats = vfs.get_fs_stats();
  std::cout << "Total blocks: " << stats.total_blocks << "\n";
  std::cout << "Free blocks: " << stats.free_blocks << "\n";
  std::cout << "Total size: " << stats.total_size << " bytes\n";

  vfs.unmount();

  // Mount again
  success = vfs.mount("/tmp/test_fs.img", 128);
  assert(success && "Mount failed");

  std::cout << "✓ Format and mount test passed\n\n";
}

void test_directory_operations() {
  std::cout << "Testing directory operations...\n";

  VirtualFileSystem vfs;
  vfs.mount("/tmp/test_fs.img", 128);

  // Create directories
  assert(vfs.mkdir("/papers") == 0);
  assert(vfs.mkdir("/users") == 0);
  assert(vfs.mkdir("/users/authors") == 0);

  // Check existence
  assert(vfs.exists("/papers"));
  assert(vfs.exists("/users/authors"));
  assert(vfs.is_directory("/papers"));

  // List root directory
  std::vector<DirEntry> entries;
  vfs.readdir("/", entries);
  std::cout << "Root directory contains " << entries.size() << " entries:\n";
  for (const auto &entry : entries) {
    std::cout << "  - " << std::string(entry.name, entry.name_len) << "\n";
  }

  std::cout << "✓ Directory operations test passed\n\n";
}

void test_file_operations() {
  std::cout << "Testing file operations...\n";

  VirtualFileSystem vfs;
  vfs.mount("/tmp/test_fs.img", 128);

  // Create a file
  const char *path = "/papers/paper1.txt";
  assert(vfs.create_file(path) == 0);
  assert(vfs.exists(path));

  // Write to file
  int fd = vfs.open(path, O_RDWR);
  assert(fd >= 0);

  const char *content = "This is a research paper about operating systems.";
  ssize_t written = vfs.write(fd, content, strlen(content));
  assert(written == static_cast<ssize_t>(strlen(content)));

  vfs.close(fd);

  // Read from file
  fd = vfs.open(path, O_RDONLY);
  assert(fd >= 0);

  char buffer[256] = {0};
  ssize_t read_bytes = vfs.read(fd, buffer, sizeof(buffer));
  assert(read_bytes == static_cast<ssize_t>(strlen(content)));
  assert(strcmp(buffer, content) == 0);

  std::cout << "Read from file: " << buffer << "\n";

  vfs.close(fd);

  std::cout << "✓ File operations test passed\n\n";
}

void test_cache_statistics() {
  std::cout << "Testing cache statistics...\n";

  VirtualFileSystem vfs;
  vfs.mount("/tmp/test_fs.img", 128);

  // Perform some file operations to trigger cache usage
  const char *path = "/test_cache.dat";
  vfs.create_file(path);

  int fd = vfs.open(path, O_RDWR);

  // Write multiple blocks
  std::vector<char> data(4096 * 10, 'A'); // 10 blocks
  vfs.write(fd, data.data(), data.size());

  vfs.close(fd);

  // Read back (should hit cache)
  fd = vfs.open(path, O_RDONLY);
  std::vector<char> read_data(data.size());
  vfs.read(fd, read_data.data(), read_data.size());
  vfs.close(fd);

  // Get cache stats
  auto cache_stats = vfs.get_cache_stats();
  std::cout << "Cache hits: " << cache_stats.hits << "\n";
  std::cout << "Cache misses: " << cache_stats.misses << "\n";
  std::cout << "Cache hit rate: " << cache_stats.hit_rate() * 100 << "%\n";
  std::cout << "Cache evictions: " << cache_stats.evictions << "\n";

  std::cout << "✓ Cache statistics test passed\n\n";
}

void test_backup_operations() {
  std::cout << "Testing backup operations...\n";

  VirtualFileSystem vfs;
  vfs.mount("/tmp/test_fs.img", 128);

  // Create some data
  vfs.mkdir("/backup_test");
  vfs.create_file("/backup_test/file1.txt");

  // Create backup
  bool success = vfs.create_backup("test_backup_1");
  assert(success && "Backup creation failed");

  std::cout << "✓ Backup test passed\n\n";
}

int main() {
  std::cout << "=== VFS Test Suite ===\n\n";

  try {
    test_format_and_mount();
    test_directory_operations();
    test_file_operations();
    test_cache_statistics();
    test_backup_operations();

    std::cout << "=== All tests passed! ===\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << "Test failed with exception: " << e.what() << "\n";
    return 1;
  }
}
