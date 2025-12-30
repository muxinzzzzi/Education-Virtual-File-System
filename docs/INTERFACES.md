# INTERFACES (Day1 Freeze)

Version: v1.0  
Last updated: 2025-12-26

---

## 0. 通用约定

### 错误码（全部接口一致）
- `0` 表示成功；负数表示错误。
- 对应关系：
| Code | Name      | 含义 |
|------|-----------|------|
| -1   | EPERM     | 无权限 / 不允许 |
| -2   | ENOENT    | 路径或条目不存在 |
| -3   | EEXIST    | 已存在 |
| -4   | ENOTDIR   | 期望目录但不是 |
| -5   | EISDIR    | 是目录但期望文件 |
| -6   | ENOTEMPTY | 目录非空 |
| -7   | EINVAL    | 参数或路径非法 |
| -8   | ENOSPC    | 空间不足（块/ inode） |
| -9   | EIO       | I/O 错误或数据损坏 |

### 路径规范（VFS 视角）
- 只接受绝对路径，必须以 `/` 开头。
- 规范化：
  - 合并多重 `/` -> `/`
  - 忽略 `.` 段
  - `..` 在 v1.0 直接拒绝（返回 `-EINVAL`），便于实现。
- `/` 为根目录。

### 线程安全
- Vfs 对外接口需内部加锁保证线程安全；调用者无需额外同步。

---

## 1. IBlockDevice（存储抽象）

**目的**：为 VFS 提供块级 I/O；VFS 不直接读写宿主文件，只通过 `IBlockDevice`。

**语义**：
- 固定块大小（全局一致），`block_id` 范围 `[0, NumBlocks()-1]`。
- `ReadBlock/WriteBlock` 必须读/写**完整一块**。
- 逻辑块 0 预留给 SuperBlock。

**接口（C++ 约定）**：
```cpp
#pragma once
#include <cstdint>
#include <cstddef>

struct IBlockDevice {
  virtual ~IBlockDevice() = default;

  virtual uint32_t BlockSize() const = 0;   // 例：4096
  virtual uint32_t NumBlocks() const = 0;   // 总块数

  // 读/写恰好一块；失败或越界返回 false。
  virtual bool ReadBlock(uint32_t block_id, void* out) = 0;
  virtual bool WriteBlock(uint32_t block_id, const void* data) = 0;

  // 将缓冲写入持久化（若有）。
  virtual bool Flush() = 0;
};
```

---

## 2. VfsAPI（Server / 业务层 -> VFS）

**生命周期**：
- `Mkfs(dev)`: 在空设备上格式化，清空旧数据。
- `Mount(dev)`: 校验 SuperBlock 并加载，返回可用 Vfs 实例。

**基本操作（返回上表错误码）**：
```cpp
class Vfs {
public:
  static bool Mkfs(std::shared_ptr<IBlockDevice> dev);
  static std::unique_ptr<Vfs> Mount(std::shared_ptr<IBlockDevice> dev);

  // 目录
  int Mkdir(const std::string& path);                      // 创建目录（父目录必须存在）
  int Rmdir(const std::string& path);                      // 仅允许删除空目录
  int ListDir(const std::string& path, std::vector<std::string>* out_names); // 不返回 . 和 ..

  // 文件
  int CreateFile(const std::string& path);                 // 创建空文件，若存在返回 -EEXIST
  int Unlink(const std::string& path);                     // 删除文件或空目录入口（目录需空）

  // 读写（按偏移）
  int ReadFile(const std::string& path, uint64_t off, uint32_t n, std::string* out); // 读到 EOF 为止
  int WriteFile(const std::string& path, uint64_t off, const std::string& data);      // 自动扩展文件

  // 可选
  int Truncate(const std::string& path, uint64_t new_size); // 缩短或扩展（扩展写 0）
};
```

**行为细节**：
- 路径在进入 Vfs 前必须先按“路径规范”处理。
- 目录项命名：不允许包含 `/` 或空字符串，长度约束由实现定义（见 `FS_LAYOUT.md`）。
- `ReadFile` 若 `off` >= 文件大小，返回 `0` 并写空数据。
- `WriteFile`、`Truncate` 需要空间时若不足返回 `-ENOSPC`（块或 inode）。
- 内部负责块缓存（LRU）、free bitmap、inode/目录解析等。

---

## 3. RPC/协议面（Client <-> Server）

- 传输、字段与示例详见 `docs/PROTOCOL.md`。
- 角色/鉴权：客户端先 `LOGIN` 获得 token，后续请求携带 `token`；服务器负责权限检查。
- 文件/目录操作直接映射到上述 VfsAPI（`MKDIR`, `LIST`, `READ`, `WRITE`, `CREATE`, `UNLINK`, `RMDIR`, `TRUNCATE`）。
- 读写使用分块传输；二进制以 base64 编码。
- 备份接口：`BACKUP_CREATE` / `BACKUP_LIST` / `BACKUP_RESTORE`（管理员可用，触发设备快照/恢复）。

