# FS_LAYOUT (VFS on IBlockDevice)

Version: v1.0  
Block size: 由设备决定（建议 4096B），所有计算基于该值。

---

## 1. 整体布局（按逻辑块编号）
- `Block 0` : SuperBlock
- `Block 1 .. BmapEnd` : Free block bitmap（覆盖全盘）
- `Block BmapEnd+1 .. InodeEnd` : Inode Table
- `Block DataStart .. end` : 数据区（文件/目录内容）

符号约定（均写入 SuperBlock）：  
- `num_blocks` : 总块数（含 SuperBlock 本身）  
- `block_size` : 块大小  
- `bmap_start` : bitmap 起始块号（通常为 1）  
- `bmap_blocks` : bitmap 占用块数，`ceil(num_blocks / (block_size*8))`  
- `inode_start` : inode 表起始块号  
- `inode_blocks` : inode 表占用块数  
- `inode_capacity` : inode 总数量 = `inode_blocks * (block_size / inode_size)`  
- `data_start` : 数据区起始块号  
- `root_inode` : 根目录 inode 号（通常为 0）

块分配范围：可分配块为 `[data_start, num_blocks-1]`，bitmap 逐位管理。

---

## 2. SuperBlock
按小端存储，结构（字段顺序固定）：
```
magic        uint32  // 'VSFS' = 0x56534653
version      uint32  // 1
block_size   uint32
num_blocks   uint32
bmap_start   uint32
bmap_blocks  uint32
inode_start  uint32
inode_blocks uint32
inode_capacity uint32
data_start   uint32
root_inode   uint32
reserved     [32]byte // 预留
```
剩余字节填 0。

---

## 3. Inode 布局
- 固定大小：`inode_size = 128` 字节。
- inode 编号：从 0 开始，`inode 0` 预留给根目录。

字段：
```
mode      uint16   // 0=free, 1=file, 2=dir
links     uint16   // 链接计数（目录至少 1）
size      uint64   // 字节数（目录表示目录数据长度）
direct[10]uint32   // 直接块指针
indirect1 uint32   // 一级间接（块号存放一组 uint32）
indirect2 uint32   // 预留（v1 未用，写 0）
reserved  [76]byte // 对齐/预留
```

块指针规则：
- 未使用指针填 0。
- 一级间接块内容：`block_size / 4` 个 `uint32` 块号。
- 文件超出 direct+indirect1 覆盖范围时返回 `-ENOSPC`（v1 不支持双重间接）。

---

## 4. 目录文件格式
- 目录本身存为文件，其数据是定长目录项数组。
- 目录项结构（64 字节）：
```
inode   uint32      // 0 表示空洞
name    char[60]    // 以 '\0' 结尾，不含 '/'，最长 59 字节
```
- 目录遍历时跳过 `inode=0` 的空项。
- 根目录初始包含 0 项（不自动放置 "." ".."）。
- `Rmdir` 仅在目录无非空项时允许删除。

---

## 5. 空闲管理与扩展
- 块分配：从 bitmap 找首个 0 置 1；释放时清 0。
- inode 分配：线性扫描 inode 表，`mode=0` 视为空闲。
- 新分配块需清零；扩大文件时自动分配并写 0。
- 缩短文件时按需释放尾部块；目录缩短时需确保不丢失有效项。

---

## 6. 块缓存与一致性
- 块缓存：LRU，容量可配置（块数），命中/未命中/淘汰计数器需可查询（供统计）。
- 写策略：v1 采用写透（write-through）；`Flush()` 仍需同步底层设备（用于持久化或备份前）。
- Mount 校验：`magic`、`version`、`block_size` 必须匹配，失败返回挂载错误。

---

## 7. 备份策略（与布局关系）
- 备份/恢复为“整设备”快照：复制全部块数据到宿主文件，恢复时整体覆盖设备。
- 因为是整盘拷贝，无需额外布局字段；但在恢复后需重新校验 SuperBlock。
