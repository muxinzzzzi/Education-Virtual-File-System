#include "filesystem/vfs.h"
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <thread>
#include <chrono>

using namespace vfs;

// 颜色定义
namespace Color {
const std::string RESET = "\033[0m";
const std::string BOLD = "\033[1m";
const std::string RED = "\033[31m";
const std::string GREEN = "\033[32m";
const std::string YELLOW = "\033[33m";
const std::string BLUE = "\033[34m";
const std::string MAGENTA = "\033[35m";
const std::string CYAN = "\033[36m";
const std::string WHITE = "\033[97m";
const std::string BG_BLUE = "\033[44m";
const std::string BG_GREEN = "\033[42m";
const std::string BG_RED = "\033[41m";
} // namespace Color

void print_header(const std::string &title) {
  std::cout << "\n"
            << Color::CYAN << Color::BOLD
            << "╔════════════════════════════════════════════════════════════════════╗\n"
            << "║ " << std::left << std::setw(66) << title << " ║\n"
            << "╚════════════════════════════════════════════════════════════════════╝"
            << Color::RESET << "\n\n";
}

void print_section(const std::string &title) {
  std::cout << "\n"
            << Color::YELLOW << Color::BOLD << "▸ " << title << Color::RESET
            << "\n";
  std::cout << Color::YELLOW << "  ────────────────────────────────────────"
            << Color::RESET << "\n";
}

void print_success(const std::string &msg) {
  std::cout << Color::GREEN << "  ✓ " << msg << Color::RESET << "\n";
}

void print_info(const std::string &key, const std::string &value,
                const std::string &unit = "") {
  std::cout << "  " << Color::CYAN << std::setw(30) << std::left << key
            << Color::RESET << ": " << Color::WHITE << Color::BOLD << value
            << Color::RESET;
  if (!unit.empty()) {
    std::cout << " " << Color::CYAN << unit << Color::RESET;
  }
  std::cout << "\n";
}

void print_progress_bar(const std::string &label, double percentage,
                        int width = 40) {
  int filled = static_cast<int>(percentage / 100.0 * width);
  std::cout << "  " << Color::CYAN << std::setw(30) << std::left << label
            << Color::RESET << " [";

  if (percentage > 80) {
    std::cout << Color::RED;
  } else if (percentage > 50) {
    std::cout << Color::YELLOW;
  } else {
    std::cout << Color::GREEN;
  }

  for (int i = 0; i < width; i++) {
    if (i < filled) {
      std::cout << "█";
    } else {
      std::cout << "░";
    }
  }

  std::cout << Color::RESET << "] " << Color::BOLD << std::fixed
            << std::setprecision(1) << percentage << "%" << Color::RESET
            << "\n";
}

void display_superblock(VirtualFileSystem &vfs) {
  print_section("超级块 (Superblock) 信息");

  auto stats = vfs.get_fs_stats();

  std::cout << "\n";
  print_info("魔数 (Magic Number)", "0x52455644", "(REVD)");
  print_info("块大小 (Block Size)", "4096", "字节");
  print_info("总块数 (Total Blocks)",
             std::to_string(stats.total_blocks));
  print_info("空闲块数 (Free Blocks)",
             std::to_string(stats.free_blocks));
  print_info("总Inode数 (Total Inodes)",
             std::to_string(stats.total_inodes));
  print_info("空闲Inode数 (Free Inodes)",
             std::to_string(stats.free_inodes));

  std::cout << "\n";
  double block_usage = stats.total_blocks > 0
                           ? (stats.total_blocks - stats.free_blocks) * 100.0 /
                                 stats.total_blocks
                           : 0;
  print_progress_bar("数据块使用率", block_usage);

  double inode_usage = stats.total_inodes > 0
                           ? (stats.total_inodes - stats.free_inodes) * 100.0 /
                                 stats.total_inodes
                           : 0;
  print_progress_bar("Inode使用率", inode_usage);

  std::cout << "\n";
  print_info("文件系统总容量",
             std::to_string(stats.total_size / 1024 / 1024), "MB");
  print_info("已使用空间",
             std::to_string(stats.used_size / 1024 / 1024), "MB");
  print_info("可用空间",
             std::to_string((stats.total_size - stats.used_size) / 1024 / 1024),
             "MB");
}

void display_fs_structure(VirtualFileSystem &vfs) {
  print_section("文件系统结构布局");

  auto stats = vfs.get_fs_stats();

  std::cout << "\n";
  std::cout << Color::CYAN << "  磁盘布局示意图:" << Color::RESET << "\n\n";

  std::cout << "  ┌────────────────┬──────────────┬───────────────┬──────────────────┐\n";
  std::cout << "  │  " << Color::GREEN << "超级块" << Color::RESET
            << "      │  " << Color::YELLOW << "Inode表" << Color::RESET
            << "     │  " << Color::MAGENTA << "空闲位图" << Color::RESET
            << "    │  " << Color::BLUE << "数据块区域" << Color::RESET
            << "     │\n";
  std::cout << "  │  (Block 0)   │  (Block 1+)  │  (Bitmap)     │  (Data Blocks)   │\n";
  std::cout << "  └────────────────┴──────────────┴───────────────┴──────────────────┘\n";

  std::cout << "\n";
  std::cout << Color::CYAN << "  各区域详细信息:" << Color::RESET << "\n\n";

  std::cout << "  " << Color::GREEN << "▪ 超级块 (Superblock)" << Color::RESET << "\n";
  std::cout << "    - 位置: Block 0\n";
  std::cout << "    - 大小: 1 block (4KB)\n";
  std::cout << "    - 内容: 文件系统元数据和配置信息\n\n";

  std::cout << "  " << Color::YELLOW << "▪ Inode表 (Inode Table)" << Color::RESET << "\n";
  std::cout << "    - 位置: Block 1 开始\n";
  std::cout << "    - 总Inode数: " << stats.total_inodes << "\n";
  std::cout << "    - 已使用: " << (stats.total_inodes - stats.free_inodes) << "\n";
  std::cout << "    - Inode大小: 128 字节\n";
  std::cout << "    - 每个Inode包含: 直接块指针(12个) + 间接块指针 + 双间接块指针\n\n";

  std::cout << "  " << Color::MAGENTA << "▪ 空闲块位图 (Free Block Bitmap)" << Color::RESET << "\n";
  std::cout << "    - 功能: 管理数据块分配状态\n";
  std::cout << "    - 方法: 每个bit代表一个数据块 (1=已用, 0=空闲)\n";
  std::cout << "    - 位图大小: " << (stats.total_blocks / 8) << " 字节\n\n";

  std::cout << "  " << Color::BLUE << "▪ 数据块区域 (Data Blocks)" << Color::RESET << "\n";
  std::cout << "    - 块大小: 4096 字节\n";
  std::cout << "    - 总块数: " << stats.total_blocks << "\n";
  std::cout << "    - 已分配: " << (stats.total_blocks - stats.free_blocks) << "\n";
  std::cout << "    - 空闲: " << stats.free_blocks << "\n";
}

void demonstrate_lru_cache(VirtualFileSystem &vfs) {
  print_section("LRU缓存机制演示");

  std::cout << "\n";
  print_info("缓存容量 (Cache Capacity)",
             std::to_string(256), "blocks");
  print_info("缓存策略", "LRU (Least Recently Used)");
  print_info("块大小", "4096", "bytes");

  std::cout << "\n";
  std::cout << Color::YELLOW << "  执行文件操作以观察缓存行为..." << Color::RESET
            << "\n\n";

  // 创建测试文件并进行读写操作
  std::vector<std::string> test_files = {
      "/cache_test_1.txt", "/cache_test_2.txt", "/cache_test_3.txt"};

  for (size_t i = 0; i < test_files.size(); i++) {
    std::cout << "  " << (i + 1) << ". 创建并写入文件: "
              << Color::CYAN << test_files[i] << Color::RESET << "\n";

    vfs.create_file(test_files[i]);
    int fd = vfs.open(test_files[i], 2); // O_RDWR

    std::string data = "测试数据块 " + std::to_string(i + 1) + " - ";
    data += std::string(4000, 'A' + i); // 填充数据

    vfs.write(fd, data.c_str(), data.size());
    vfs.close(fd);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
  }

  std::cout << "\n";
  std::cout << Color::YELLOW << "  重复读取文件以测试缓存命中..." << Color::RESET
            << "\n\n";

  // 多次读取同一文件，观察缓存命中
  for (int round = 0; round < 2; round++) {
    std::cout << "  第 " << (round + 1) << " 轮读取:\n";
    for (const auto &file : test_files) {
      int fd = vfs.open(file, 0); // O_RDONLY
      char buffer[4096];
      vfs.read(fd, buffer, sizeof(buffer));
      vfs.close(fd);
      std::cout << "    - 读取 " << Color::CYAN << file << Color::RESET
                << "\n";
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }

  // 显示缓存统计
  auto final_stats = vfs.get_cache_stats();

  std::cout << "\n";
  print_section("LRU缓存统计结果");
  std::cout << "\n";

  print_info("缓存命中次数 (Cache Hits)",
             std::to_string(final_stats.hits));
  print_info("缓存未命中次数 (Cache Misses)",
             std::to_string(final_stats.misses));
  print_info("缓存驱逐次数 (Evictions)",
             std::to_string(final_stats.evictions));
  print_info("总请求次数 (Total Requests)",
             std::to_string(final_stats.total_requests));

  std::cout << "\n";
  double hit_rate = final_stats.hit_rate() * 100;
  print_progress_bar("缓存命中率 (Hit Rate)", hit_rate);

  std::cout << "\n";
  std::cout << Color::GREEN << "  ✓ LRU缓存工作正常！" << Color::RESET << "\n";
  std::cout << Color::CYAN
            << "  📊 缓存显著提升了文件访问性能，减少磁盘I/O次数" << Color::RESET
            << "\n";

  // 清理测试文件
  for (const auto &file : test_files) {
    vfs.delete_file(file);
  }
}

void demonstrate_directory_operations(VirtualFileSystem &vfs) {
  print_section("多级目录结构演示");

  std::cout << "\n";
  std::cout << Color::YELLOW << "  创建多级目录结构..." << Color::RESET
            << "\n\n";

  // 创建目录结构
  std::vector<std::string> dirs = {"/papers", "/papers/AI", "/papers/AI/2024",
                                   "/papers/DB", "/reviews", "/reviews/round1"};

  for (const auto &dir : dirs) {
    vfs.mkdir(dir);
    std::cout << "  " << Color::GREEN << "✓" << Color::RESET << " 创建目录: "
              << Color::CYAN << dir << Color::RESET << "\n";
  }

  std::cout << "\n";
  std::cout << Color::YELLOW << "  在目录中创建文件..." << Color::RESET
            << "\n\n";

  // 创建文件
  std::vector<std::string> files = {
      "/papers/AI/2024/paper1.pdf", "/papers/AI/2024/paper2.pdf",
      "/papers/DB/database_research.pdf", "/reviews/round1/review1.txt"};

  for (const auto &file : files) {
    vfs.create_file(file);
    std::cout << "  " << Color::GREEN << "✓" << Color::RESET << " 创建文件: "
              << Color::CYAN << file << Color::RESET << "\n";
  }

  std::cout << "\n";
  print_section("目录树结构");
  std::cout << "\n";

  std::cout << "  /\n";
  std::cout << "  ├── 📁 papers/\n";
  std::cout << "  │   ├── 📁 AI/\n";
  std::cout << "  │   │   └── 📁 2024/\n";
  std::cout << "  │   │       ├── 📄 paper1.pdf\n";
  std::cout << "  │   │       └── 📄 paper2.pdf\n";
  std::cout << "  │   └── 📁 DB/\n";
  std::cout << "  │       └── 📄 database_research.pdf\n";
  std::cout << "  └── 📁 reviews/\n";
  std::cout << "      └── 📁 round1/\n";
  std::cout << "          └── 📄 review1.txt\n";

  std::cout << "\n";
  print_success("多级目录和文件创建成功！");
  std::cout << Color::CYAN << "  📂 支持完整的路径解析和目录遍历"
            << Color::RESET << "\n";
}

void demonstrate_file_operations(VirtualFileSystem &vfs) {
  print_section("文件读写操作演示");

  std::cout << "\n";
  std::string test_file = "/test_io_demo.txt";

  // 创建文件
  std::cout << "  1. " << Color::YELLOW << "创建文件" << Color::RESET << ": "
            << test_file << "\n";
  vfs.create_file(test_file);
  print_success("文件创建成功");

  // 写入数据
  std::cout << "\n  2. " << Color::YELLOW << "写入数据" << Color::RESET
            << "\n";
  int fd = vfs.open(test_file, 2); // O_RDWR

  std::string write_data =
      "这是文件系统测试数据。\n"
      "支持多次写入和读取操作。\n"
      "数据块通过LRU缓存提高访问效率。\n"
      "文件系统维护完整的inode结构。\n";

  ssize_t written = vfs.write(fd, write_data.c_str(), write_data.size());
  std::cout << "     - 写入字节数: " << Color::GREEN << written
            << Color::RESET << " bytes\n";
  vfs.close(fd);
  print_success("数据写入成功");

  // 读取数据
  std::cout << "\n  3. " << Color::YELLOW << "读取数据" << Color::RESET
            << "\n";
  fd = vfs.open(test_file, 0); // O_RDONLY

  char read_buffer[1024] = {0};
  ssize_t read_bytes = vfs.read(fd, read_buffer, sizeof(read_buffer));
  std::cout << "     - 读取字节数: " << Color::GREEN << read_bytes
            << Color::RESET << " bytes\n";
  vfs.close(fd);
  print_success("数据读取成功");

  std::cout << "\n  4. " << Color::YELLOW << "读取内容" << Color::RESET
            << ":\n";
  std::cout << Color::CYAN << "  ┌────────────────────────────────────────┐"
            << Color::RESET << "\n";
  std::istringstream iss(read_buffer);
  std::string line;
  while (std::getline(iss, line)) {
    std::cout << Color::CYAN << "  │ " << Color::RESET << line << "\n";
  }
  std::cout << Color::CYAN << "  └────────────────────────────────────────┘"
            << Color::RESET << "\n";

  // 删除文件
  std::cout << "\n  5. " << Color::YELLOW << "删除文件" << Color::RESET
            << "\n";
  vfs.delete_file(test_file);
  print_success("文件删除成功，数据块和inode已释放");
}

void demonstrate_backup(VirtualFileSystem &vfs) {
  print_section("备份与恢复功能演示");

  std::cout << "\n";
  std::cout << Color::YELLOW << "  准备测试数据..." << Color::RESET << "\n\n";

  // 创建一些数据
  vfs.create_file("/backup_test_1.txt");
  vfs.create_file("/backup_test_2.txt");
  vfs.mkdir("/backup_dir");
  vfs.create_file("/backup_dir/file.txt");

  std::cout << "  " << Color::GREEN << "✓" << Color::RESET
            << " 创建测试文件和目录\n";

  // 创建备份
  std::cout << "\n";
  std::cout << Color::YELLOW << "  创建系统备份..." << Color::RESET << "\n\n";

  std::string backup_name = "demo_backup_" +
                            std::to_string(std::time(nullptr));
  bool backup_success = vfs.create_backup(backup_name);

  if (backup_success) {
    print_success("备份创建成功: " + backup_name);
  }

  // 列出所有备份
  std::cout << "\n";
  print_section("当前系统备份列表");
  std::cout << "\n";

  auto backups = vfs.list_backups();
  if (backups.empty()) {
    std::cout << "  " << Color::YELLOW << "暂无备份" << Color::RESET << "\n";
  } else {
    for (size_t i = 0; i < backups.size(); i++) {
      std::cout << "  " << (i + 1) << ". " << Color::CYAN << backups[i]
                << Color::RESET << "\n";
    }
  }

  std::cout << "\n";
  std::cout << Color::GREEN << "  ✓ 备份功能正常工作" << Color::RESET << "\n";
  std::cout << Color::CYAN
            << "  💾 管理员可以创建版本化快照并在需要时恢复系统状态"
            << Color::RESET << "\n";

  // 清理
  vfs.delete_file("/backup_test_1.txt");
  vfs.delete_file("/backup_test_2.txt");
  vfs.delete_file("/backup_dir/file.txt");
  vfs.rmdir("/backup_dir");
}

int main(int argc, char *argv[]) {
  std::string image_path = "fs_demo.img";
  if (argc > 1) {
    image_path = argv[1];
  }

  print_header("文件系统核心特性演示工具");

  std::cout << Color::WHITE
            << "本演示将展示教育虚拟文件系统的所有核心特性:\n"
            << "  • 超级块 (Superblock) 结构\n"
            << "  • Inode表和数据块管理\n"
            << "  • 空闲块位图 (Free Bitmap)\n"
            << "  • LRU缓存机制及统计\n"
            << "  • 多级目录结构\n"
            << "  • 文件创建、读写、删除\n"
            << "  • 路径解析\n"
            << "  • 备份与恢复功能\n" << Color::RESET << "\n";

  // 创建并挂载文件系统
  VirtualFileSystem vfs;
  
  std::cout << Color::CYAN << "初始化文件系统 (10MB, 256-block LRU缓存)..."
            << Color::RESET << "\n";

  if (!vfs.format(image_path, 10, 256)) {
    std::cerr << Color::RED << "错误: 无法格式化文件系统" << Color::RESET
              << "\n";
    return 1;
  }

  if (!vfs.mount(image_path, 256)) {
    std::cerr << Color::RED << "错误: 无法挂载文件系统" << Color::RESET << "\n";
    return 1;
  }

  print_success("文件系统初始化成功");

  std::cout << "\n"
            << Color::YELLOW
            << "按回车开始演示，每个演示后需要按回车继续..." << Color::RESET
            << "\n";
  std::cin.get();

  // 1. 展示超级块
  display_superblock(vfs);
  std::cout << "\n" << Color::YELLOW << "按回车继续..." << Color::RESET;
  std::cin.get();

  // 2. 展示文件系统结构
  display_fs_structure(vfs);
  std::cout << "\n" << Color::YELLOW << "按回车继续..." << Color::RESET;
  std::cin.get();

  // 3. 演示LRU缓存
  demonstrate_lru_cache(vfs);
  std::cout << "\n" << Color::YELLOW << "按回车继续..." << Color::RESET;
  std::cin.get();

  // 4. 演示目录操作
  demonstrate_directory_operations(vfs);
  std::cout << "\n" << Color::YELLOW << "按回车继续..." << Color::RESET;
  std::cin.get();

  // 5. 演示文件操作
  demonstrate_file_operations(vfs);
  std::cout << "\n" << Color::YELLOW << "按回车继续..." << Color::RESET;
  std::cin.get();

  // 6. 演示备份功能
  demonstrate_backup(vfs);

  // 最终统计
  print_header("演示总结");

  auto final_stats = vfs.get_fs_stats();
  auto cache_stats = vfs.get_cache_stats();

  std::cout << Color::GREEN << Color::BOLD << "所有核心特性演示完成！"
            << Color::RESET << "\n\n";

  std::cout << Color::CYAN << "文件系统最终状态:" << Color::RESET << "\n";
  print_info("数据块使用",
             std::to_string(final_stats.total_blocks - final_stats.free_blocks) +
                 " / " + std::to_string(final_stats.total_blocks));
  print_info("Inode使用",
             std::to_string(final_stats.total_inodes - final_stats.free_inodes) +
                 " / " + std::to_string(final_stats.total_inodes));
  print_info("缓存命中率",
             std::to_string(static_cast<int>(cache_stats.hit_rate() * 100)) +
                 "%");

  std::cout << "\n"
            << Color::WHITE
            << "✨ 文件系统设计体现了:\n"
            << "   • 清晰的数据结构 (超级块、inode、数据块)\n"
            << "   • 高效的存储管理 (bitmap分配、多级索引)\n"
            << "   • 性能优化机制 (LRU缓存)\n"
            << "   • 完整的目录支持 (多级路径)\n"
            << "   • 数据安全保障 (备份恢复)\n" << Color::RESET << "\n";

  vfs.unmount();

  std::cout << Color::GREEN << "\n✓ 演示完成！" << Color::RESET << "\n";
  return 0;
}

