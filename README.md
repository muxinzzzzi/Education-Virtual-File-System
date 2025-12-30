# VFS (虚拟文件系统) - 模块 A

## 项目简介

这是一个完整实现的虚拟文件系统（VFS），支持操作系统实训课程要求的所有核心功能。

## 已实现功能

### 1. 磁盘布局与持久化 ✅
- **SuperBlock**: 包含文件系统元数据（magic, version, 布局信息）
- **Inode Table**: 固定大小 128 字节，支持文件和目录
- **Free Bitmap**: 管理数据块的分配与释放
- **Data Blocks**: 支持直接块（10个）+ 一级间接块

### 2. 目录与路径管理 ✅
- **多级目录**: 支持任意深度的目录树
- **路径解析**: 自动处理绝对路径，规范化多重斜杠
- **Mkdir/Rmdir**: 创建和删除目录（仅空目录可删）
- **Lookup**: 高效的路径查找，支持父目录定位

### 3. 文件读写 ✅
- **Create/Unlink**: 创建和删除文件
- **ReadFile/WriteFile**: 按偏移量读写，支持自动扩展
- **Truncate**: 可选功能，支持文件截断和扩展
- **大文件支持**: 通过间接块支持超过 40KB 的文件
- **稀疏文件**: 支持跨块写入，自动填充零

### 4. Server API 接口 ✅
统一的 VFS API 对外提供：
- `Vfs::Mkfs(dev)` - 格式化文件系统
- `Vfs::Mount(dev)` - 挂载文件系统
- `Mkdir/Rmdir/ListDir` - 目录操作
- `CreateFile/Unlink` - 文件操作  
- `ReadFile/WriteFile` - 读写操作
- `Truncate` - 可选操作

### 5. 线程安全 ✅
所有公共接口内部加锁，保证并发安全。

### 6. 错误处理 ✅
返回标准错误码：
- `VFS_EPERM` (-1): 无权限
- `VFS_ENOENT` (-2): 路径不存在
- `VFS_EEXIST` (-3): 已存在
- `VFS_ENOTDIR` (-4): 期望目录但不是
- `VFS_EISDIR` (-5): 是目录但期望文件
- `VFS_ENOTEMPTY` (-6): 目录非空
- `VFS_EINVAL` (-7): 参数或路径非法
- `VFS_ENOSPC` (-8): 空间不足
- `VFS_EIO` (-9): I/O 错误

## 项目结构

```
OS_work/
├── vfs/
│   ├── include/
│   │   ├── block_device.h    # 块设备抽象接口
│   │   └── vfs.h              # VFS 公共接口
│   ├── src/
│   │   ├── mem_block_device.cc  # 内存块设备实现（测试用）
│   │   └── vfs.cc               # VFS 核心实现（~770行）
│   └── tests/
│       └── test_vfs.cc        # 12个单元测试
├── docs/
│   ├── INTERFACES.md          # 接口规范（Day1冻结）
│   ├── PROTOCOL.md            # 客户端-服务器协议
│   ├── FS_LAYOUT.md           # 文件系统布局详细说明
│   └── REQUIREMENTS.md        # 项目需求
├── build/                     # 编译输出目录
│   ├── libvfs.a              # VFS 静态库
│   └── test_vfs              # 测试可执行文件
├── CMakeLists.txt            # CMake 构建配置
└── README.md                 # 本文档
```

## 编译与测试

### 1. 编译项目

```bash
cd /Users/muxin/Desktop/OS_work
mkdir -p build
cd build
cmake ..
make
```

编译成功后会生成：
- `libvfs.a` - VFS 静态库
- `test_vfs` - 测试程序

### 2. 运行测试

**方式一：运行完整测试套件**
```bash
cd /Users/muxin/Desktop/OS_work/build
./test_vfs
```

测试包含 12 个测试用例：
1. ✅ Mkfs 和 Mount 基本功能
2. ✅ Mkdir 和 ListDir
3. ✅ 嵌套目录创建
4. ✅ 文件创建和删除
5. ✅ 小文件读写
6. ✅ 大文件多块读写
7. ✅ 目录删除（空/非空）
8. ✅ 路径查找错误处理
9. ✅ Truncate 文件截断
10. ✅ 批量文件创建（bitmap分配）
11. ✅ 大文件使用间接块
12. ✅ 稀疏文件写入

**方式二：单独测试某个功能**

创建自己的测试文件：
```cpp
#include "vfs.h"
#include "block_device.h"
#include <iostream>

int main() {
  auto dev = MakeMemBlockDevice(1024, 4096);
  Vfs::Mkfs(dev);
  auto vfs = Vfs::Mount(dev);
  
  vfs->Mkdir("/papers");
  vfs->CreateFile("/papers/test.txt");
  vfs->WriteFile("/papers/test.txt", 0, "Hello VFS!");
  
  std::string content;
  vfs->ReadFile("/papers/test.txt", 0, 100, &content);
  std::cout << "Read: " << content << "\n";
  
  return 0;
}
```

编译运行：
```bash
g++ -std=c++17 -I vfs/include my_test.cc build/libvfs.a vfs/src/mem_block_device.cc -o my_test
./my_test
```

### 3. 集成到 Server 端

在 Server 代码中引入 VFS：

```cpp
#include "vfs.h"
#include "block_device.h"

// 初始化
auto dev = MakeMemBlockDevice(8192, 4096); // 32MB 虚拟磁盘
Vfs::Mkfs(dev);
auto vfs = Vfs::Mount(dev);

// 使用示例
vfs->Mkdir("/papers");
vfs->CreateFile("/papers/paper_123/v1.pdf");
vfs->WriteFile("/papers/paper_123/v1.pdf", 0, pdf_data);

std::vector<std::string> papers;
vfs->ListDir("/papers", &papers);
```

## 技术细节

### 磁盘布局
```
Block 0:          SuperBlock
Block 1..N:       Free Bitmap  
Block N+1..M:     Inode Table (1024 个 inode)
Block M+1..End:   Data Blocks
```

### Inode 结构（128字节）
- mode (2B): 0=空闲, 1=文件, 2=目录
- links (2B): 链接计数
- size (8B): 文件/目录大小（字节）
- direct[10] (40B): 10个直接块指针
- indirect1 (4B): 一级间接块指针
- indirect2 (4B): 预留（未使用）
- reserved (68B): 对齐填充

### 目录项结构（64字节）
- inode (4B): inode号，0表示空洞
- name[60]: 文件/目录名，以'\0'结尾

### 块分配策略
- 首次适配（First-fit）：从 bitmap 中找到第一个空闲块
- 新分配块自动清零
- 释放时清除 bitmap 对应位

## 性能特点

- ✅ 支持文件大小：最大 ~4MB（10个直接块 + 1024个间接块）
- ✅ 支持 inode 数量：1024 个（可配置）
- ✅ 块大小：4096 字节（标准页大小）
- ✅ 线程安全：全局互斥锁
- ✅ 写策略：写透（write-through）

## 下一步工作

根据项目要求，模块 A (VFS) 已完成。接下来需要：

1. **模块 B (Server/Protocol)**
   - 实现 TCP 服务器
   - 实现 JSON 协议解析
   - 连接 VFS API
   - 实现用户认证和权限管理
   - 实现四种角色（author/reviewer/editor/admin）

2. **模块 C (Client CLI)**
   - 实现交互式命令行界面
   - 实现文件上传/下载
   - 实现分块传输

3. **备份功能**
   - 实现设备快照
   - 实现备份列表管理
   - 实现恢复功能

## 参考文档

详细设计见 `docs/` 目录：
- `INTERFACES.md` - 接口定义和错误码
- `FS_LAYOUT.md` - 磁盘布局和数据结构
- `PROTOCOL.md` - 网络协议规范
- `REQUIREMENTS.md` - 项目需求清单

## 作者

模块 A 负责人：成员 1
完成时间：2025-12-26

