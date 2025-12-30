#include "vfs.h"
#include "block_device.h"
#include <iostream>
#include <cassert>
#include <cstring>

void test_mkfs_mount() {
  std::cout << "Test 1: Mkfs and Mount... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  assert(Vfs::Mkfs(dev));
  auto vfs = Vfs::Mount(dev);
  assert(vfs != nullptr);
  std::cout << "PASS\n";
}

void test_mkdir_listdir() {
  std::cout << "Test 2: Mkdir and ListDir... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->Mkdir("/papers") == 0);
  assert(vfs->Mkdir("/reviews") == 0);
  assert(vfs->Mkdir("/papers") == VFS_EEXIST);
  
  std::vector<std::string> names;
  assert(vfs->ListDir("/", &names) == 0);
  assert(names.size() == 2);
  assert((names[0] == "papers" && names[1] == "reviews") || 
         (names[0] == "reviews" && names[1] == "papers"));
  
  std::cout << "PASS\n";
}

void test_nested_mkdir() {
  std::cout << "Test 3: Nested directory creation... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->Mkdir("/papers") == 0);
  assert(vfs->Mkdir("/papers/p1") == 0);
  assert(vfs->Mkdir("/papers/p1/versions") == 0);
  
  std::vector<std::string> names;
  assert(vfs->ListDir("/papers", &names) == 0);
  assert(names.size() == 1);
  assert(names[0] == "p1");
  
  assert(vfs->ListDir("/papers/p1", &names) == 0);
  assert(names.size() == 1);
  assert(names[0] == "versions");
  
  std::cout << "PASS\n";
}

void test_create_unlink() {
  std::cout << "Test 4: Create and Unlink files... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->Mkdir("/papers") == 0);
  assert(vfs->CreateFile("/papers/paper1.txt") == 0);
  assert(vfs->CreateFile("/papers/paper2.txt") == 0);
  assert(vfs->CreateFile("/papers/paper1.txt") == VFS_EEXIST);
  
  std::vector<std::string> names;
  assert(vfs->ListDir("/papers", &names) == 0);
  assert(names.size() == 2);
  
  assert(vfs->Unlink("/papers/paper1.txt") == 0);
  assert(vfs->ListDir("/papers", &names) == 0);
  assert(names.size() == 1);
  assert(names[0] == "paper2.txt");
  
  assert(vfs->Unlink("/papers/paper1.txt") == VFS_ENOENT);
  
  std::cout << "PASS\n";
}

void test_read_write_small() {
  std::cout << "Test 5: Read/Write small files... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->CreateFile("/test.txt") == 0);
  
  std::string data = "Hello, VFS!";
  assert(vfs->WriteFile("/test.txt", 0, data) == 0);
  
  std::string out;
  assert(vfs->ReadFile("/test.txt", 0, 100, &out) == 0);
  assert(out == data);
  
  // Read partial
  assert(vfs->ReadFile("/test.txt", 7, 3, &out) == 0);
  assert(out == "VFS");
  
  // Write at offset
  assert(vfs->WriteFile("/test.txt", 7, "World!") == 0);
  assert(vfs->ReadFile("/test.txt", 0, 100, &out) == 0);
  assert(out == "Hello, World!");
  
  std::cout << "PASS\n";
}

void test_read_write_large() {
  std::cout << "Test 6: Read/Write large files (multi-block)... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->CreateFile("/large.bin") == 0);
  
  // Write 12KB across 3 blocks
  std::string data(12000, 'A');
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = 'A' + (i % 26);
  }
  
  assert(vfs->WriteFile("/large.bin", 0, data) == 0);
  
  std::string out;
  assert(vfs->ReadFile("/large.bin", 0, 12000, &out) == 0);
  assert(out == data);
  
  // Read from middle
  assert(vfs->ReadFile("/large.bin", 5000, 2000, &out) == 0);
  assert(out.size() == 2000);
  assert(out == data.substr(5000, 2000));
  
  std::cout << "PASS\n";
}

void test_rmdir() {
  std::cout << "Test 7: Rmdir (empty and non-empty)... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->Mkdir("/dir1") == 0);
  assert(vfs->Mkdir("/dir2") == 0);
  assert(vfs->CreateFile("/dir2/file.txt") == 0);
  
  // Can remove empty dir
  assert(vfs->Rmdir("/dir1") == 0);
  
  // Cannot remove non-empty dir
  assert(vfs->Rmdir("/dir2") == VFS_ENOTEMPTY);
  
  // Remove file then dir
  assert(vfs->Unlink("/dir2/file.txt") == 0);
  assert(vfs->Rmdir("/dir2") == 0);
  
  std::vector<std::string> names;
  assert(vfs->ListDir("/", &names) == 0);
  assert(names.size() == 0);
  
  std::cout << "PASS\n";
}

void test_path_lookup_errors() {
  std::cout << "Test 8: Path lookup error handling... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  // Non-existent path
  std::vector<std::string> names;
  assert(vfs->ListDir("/nonexist", &names) == VFS_ENOENT);
  
  // Create file then try to use as directory
  assert(vfs->CreateFile("/file.txt") == 0);
  assert(vfs->Mkdir("/file.txt/subdir") == VFS_ENOTDIR);
  assert(vfs->ListDir("/file.txt", &names) == VFS_ENOTDIR);
  
  // Try to read directory as file
  assert(vfs->Mkdir("/dir") == 0);
  std::string out;
  assert(vfs->ReadFile("/dir", 0, 10, &out) == VFS_EISDIR);
  assert(vfs->WriteFile("/dir", 0, "data") == VFS_EISDIR);
  
  std::cout << "PASS\n";
}

void test_truncate() {
  std::cout << "Test 9: Truncate file... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->CreateFile("/test.txt") == 0);
  assert(vfs->WriteFile("/test.txt", 0, "Hello, World!") == 0);
  
  // Shrink
  assert(vfs->Truncate("/test.txt", 5) == 0);
  std::string out;
  assert(vfs->ReadFile("/test.txt", 0, 100, &out) == 0);
  assert(out == "Hello");
  
  // Extend with zeros
  assert(vfs->Truncate("/test.txt", 10) == 0);
  assert(vfs->ReadFile("/test.txt", 0, 100, &out) == 0);
  assert(out.size() == 10);
  assert(out == std::string("Hello\0\0\0\0\0", 10));
  
  std::cout << "PASS\n";
}

void test_many_files() {
  std::cout << "Test 10: Create many files (bitmap allocation)... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->Mkdir("/files") == 0);
  
  // Create 50 small files
  for (int i = 0; i < 50; ++i) {
    std::string name = "/files/file" + std::to_string(i) + ".txt";
    assert(vfs->CreateFile(name) == 0);
    std::string data = "Content " + std::to_string(i);
    assert(vfs->WriteFile(name, 0, data) == 0);
  }
  
  std::vector<std::string> names;
  assert(vfs->ListDir("/files", &names) == 0);
  assert(names.size() == 50);
  
  // Verify content
  for (int i = 0; i < 50; ++i) {
    std::string name = "/files/file" + std::to_string(i) + ".txt";
    std::string out;
    assert(vfs->ReadFile(name, 0, 100, &out) == 0);
    assert(out == "Content " + std::to_string(i));
  }
  
  // Delete half
  for (int i = 0; i < 25; ++i) {
    std::string name = "/files/file" + std::to_string(i) + ".txt";
    assert(vfs->Unlink(name) == 0);
  }
  
  assert(vfs->ListDir("/files", &names) == 0);
  assert(names.size() == 25);
  
  std::cout << "PASS\n";
}

void test_indirect_blocks() {
  std::cout << "Test 11: Large file using indirect blocks... ";
  auto dev = MakeMemBlockDevice(4096, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->CreateFile("/bigfile.bin") == 0);
  
  // Write beyond direct blocks (10 * 4096 = 40KB)
  // Write 50KB to trigger indirect block usage
  std::string data(50000, 'X');
  for (size_t i = 0; i < data.size(); ++i) {
    data[i] = 'A' + (i % 26);
  }
  
  assert(vfs->WriteFile("/bigfile.bin", 0, data) == 0);
  
  std::string out;
  assert(vfs->ReadFile("/bigfile.bin", 0, 50000, &out) == 0);
  assert(out == data);
  
  // Read across indirect boundary
  assert(vfs->ReadFile("/bigfile.bin", 39000, 5000, &out) == 0);
  assert(out == data.substr(39000, 5000));
  
  std::cout << "PASS\n";
}

void test_write_sparse() {
  std::cout << "Test 12: Sparse file writes... ";
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  assert(vfs->CreateFile("/sparse.bin") == 0);
  
  // Write at offset 8000 (beyond first block)
  std::string data = "Hello";
  assert(vfs->WriteFile("/sparse.bin", 8000, data) == 0);
  
  // Read before written area (should be zeros)
  std::string out;
  assert(vfs->ReadFile("/sparse.bin", 0, 100, &out) == 0);
  assert(out == std::string(100, '\0'));
  
  // Read written area
  assert(vfs->ReadFile("/sparse.bin", 8000, 5, &out) == 0);
  assert(out == "Hello");
  
  std::cout << "PASS\n";
}

int main() {
  std::cout << "=== VFS Test Suite ===\n\n";
  
  try {
    test_mkfs_mount();
    test_mkdir_listdir();
    test_nested_mkdir();
    test_create_unlink();
    test_read_write_small();
    test_read_write_large();
    test_rmdir();
    test_path_lookup_errors();
    test_truncate();
    test_many_files();
    test_indirect_blocks();
    test_write_sparse();
    
    std::cout << "\n=== All 12 tests PASSED ===\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "\nTest FAILED with exception: " << e.what() << "\n";
    return 1;
  }
}
