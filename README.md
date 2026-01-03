# Peer Review System

完整的科研审稿系统，使用 C++ 实现，包含自定义文件系统、Client-Server 架构和多角色权限管理。

## 系统架构

### 核心组件

1. **虚拟文件系统 (VFS)**
   -自定义文件系统，包含 superblock、inode、bitmap
   - LRU 缓存，提升性能
   - 完整的文件与目录操作
   - 备份与恢复功能

2. **网络服务器**
   - 基于 TCP/IP 的多线程服务器
   - 用户认证与授权
   - 四种用户角色：作者、审稿人、编辑、管理员
   - 完整的审稿业务逻辑

3. **CLI 客户端**
   - 交互式命令行界面
   - 基于角色的功能菜单
   - 文件上传下载
   - 状态查看

## 编译与运行

### 环境要求

- **操作系统**: macOS 或 Linux
- **编译器**: 支持 C++17 的 Clang 或 GCC
- **构建工具**: CMake 3.16+
- **依赖库**: OpenSSL, Threads, Boost.Asio (如有使用), Qt 6 (GUI)

### 快速编译

```bash
# 1. 创建并进入构建目录
mkdir -p build && cd build

# 2. 配置项目 (默认不包含 GUI，如需 GUI 请添加 -DBUILD_GUI=ON)
cmake ..

# 3. 编译所有组件
make -j$(sysctl -n hw.ncpu || nproc)
```

### 运行指令

#### 1. 启动服务器 (必须首先运行)

服务器负责管理 VFS 镜像和处理所有业务逻辑。

```bash
# 格式: ./src/server/review_server [端口] [VFS镜像路径]
cd build
./src/server/review_server 8080 review_system.img
```

#### 2. 启动 CLI 客户端

在另一个终端中运行客户端进行交互。

```bash
# 格式: ./src/client/review_client [服务器IP] [端口]
cd build
./src/client/review_client 127.0.0.1 8080
```
#### 3. 启动 GUI 客户端
未完成完，运行需要先下载 Qt 6

```bash
./run_gui.sh
```

---

## 详细测试流程 (CLI 示例)
还有一些其他的功能也能测试

### 阶段 1: 作者提交论文
1. 使用 **alice / password** 登录客户端。
2. 选择 `1. Upload Paper`。
3. 输入标题（如 `AI Research`）和本地文件路径。
4. 系统返回 `201 Created` 及 `paper_id=P1`。
5. 选择 `3. Logout`。

### 阶段 2: 编辑分配审稿人
1. 使用 **charlie / password** 登录客户端。
2. 选择 `1. Assign Reviewer`。
3. 输入 Paper ID: `P1`，审稿人用户名: `bob`。
4. 选择 `3. Logout`。

### 阶段 3: 审稿人评审
1. 使用 **bob / password** 登录客户端。
2. 选择 `1. Download Paper`。输入 `P1` 和本地保存路径。
3. 选择 `2. Submit Review`。输入 `P1` 和包含意见的本地文件路径。
4. 选择 `3. Logout`。

### 阶段 4: 编辑做出决定
1. 使用 **charlie / password** 登录。
2. 选择 `2. Make Decision`。
3. 输入 `P1`，决定输入 `accept`。
4. 系统更新论文状态。

### 阶段 5: 管理员维护
1. 使用 **admin / admin123** 登录。
2. 选择 `2. View System Status` 查看 VFS 运行统计。
3. 选择 `3. Create Backup` 输入备份名 `stable_v1`。
4. 系统将在 VFS 内部创建磁盘快照。

## 用户角色与功能

### 默认测试账户

- **管理员**: admin / admin123
- **作者**: alice / password
- **审稿人**: bob / password
- **编辑**: charlie / password

### 功能列表

- **作者**: 上传论文、提交修订版本、查看审稿状态、下载审稿意见
- **审稿人**: 下载论文、上传评审意见、查看审稿状态
- **编辑**: 分配审稿人、查看审稿状态、作出最终决定
- **管理员**: 用户管理、备份管理、系统状态查看

## 要添加的功能

- **现有功能检查**
   - **可能有问题**: 三个查看审稿状态应该做区分, 比如审稿人的查看审稿状态应该是能看到分配给自己的文章，然后再输入ID查看。

- **审稿业务逻辑完善（已实现）**
   - **多轮审稿轮次管理**：Round1/ Round2/ Rebuttal，提交修订会推进轮次，决策为 Major/Minor 时进入 Rebuttal。
   - **审稿意见匿名化策略**：上传/分配时可选 `single` 或 `double`，作者侧下载评审/查看状态时自动隐藏审稿人身份。
   - **最终决定状态细分**：Accept / Minor / Major / Reject，状态写入 `status.json` 并在查看状态时展示。

- **自动化与智能化分配**
    - **Conflict of Interest (COI) 检测**
    - **基于关键词/领域的自动审稿人推荐**
    - **负载均衡（避免某个审稿人任务过多）**

- **高级文件系统特性（已实现基础版）**
   - **日志型文件系统（Journaling）**：写前日志自动重放，`SYSTEM_STATUS` 展示重放统计。
   - **Copy-on-Write 快照备份**：管理员 `Create Backup` 生成 COW 快照，`List Backups`/`Restore Backup` 使用差分文件回滚。
   - **崩溃恢复与校验**：块级 checksum 保存到 `.checksum`，读时校验并告警。

- **GUI 客户端**