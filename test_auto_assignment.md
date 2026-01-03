# 自动审稿人分配功能测试指南

## 编译与启动

### 1. 编译项目
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System
mkdir -p build && cd build
cmake ..
make -j$(sysctl -n hw.ncpu)
```

### 2. 启动服务器
```bash
cd build
./src/server/review_server 8080 test_assignment.img
```

### 3. 启动客户端（另一个终端）
```bash
cd build
./src/client/review_client 127.0.0.1 8080
```

---

## 测试用例

### 测试用例 1: 设置审稿人 Profile

**目标**: 为审稿人设置研究领域和关键词

**步骤**:
1. 使用 `bob / password` 登录（Reviewer 角色）
2. 选择 `4. Set My Profile`
3. 输入:
   - Fields: `AI,Machine Learning,Computer Vision`
   - Keywords: `deep learning,neural networks,image recognition`
   - Affiliation: `MIT`
4. 选择 `5. View My Profile` 验证保存成功

**预期结果**: 
- Profile 成功保存到 VFS `/users/bob/profile.txt`
- 查看时能看到刚才输入的信息

---

### 测试用例 2: 上传带有领域/关键词的论文

**目标**: 作者上传论文时指定研究领域和关键词

**步骤**:
1. 退出 bob，使用 `alice / password` 登录（Author 角色）
2. 选择 `1. Upload Paper`
3. 输入:
   - Title: `Deep Learning for Image Classification`
   - File path: `/path/to/test.pdf` (任意文本文件即可)
   - 如果客户端支持，还可以输入 fields 和 keywords
4. 记录返回的 paper_id (例如 P1)

**预期结果**:
- 论文成功上传
- Metadata 保存到 `/papers/P1/metadata.txt`

**手动补充 metadata（如果客户端未实现输入）**:
可以通过修改服务器端 VFS 文件来模拟：
```bash
# 在服务器运行时，metadata 已经创建
# 可以通过 Editor 或 Admin 角色查看
```

---

### 测试用例 3: COI 检测 - 作者不能审自己的论文

**目标**: 验证 COI 规则：reviewer == author

**步骤**:
1. 退出 alice，使用 `charlie / password` 登录（Editor 角色）
2. 选择 `1. Assign Reviewer (Manual)`
3. 输入:
   - Paper ID: `P1`
   - Reviewer: `alice` (论文作者)

**预期结果**:
- 返回错误: `409 Conflict: COI detected: Reviewer is the paper author`
- 分配失败

---

### 测试用例 4: COI 检测 - 相同机构

**目标**: 验证 COI 规则：同一 affiliation

**前置条件**: 
1. 先为 alice 设置 profile（需要先创建 alice 的 reviewer profile）
2. alice 和 bob 都设置 affiliation 为 `MIT`

**步骤**:
1. 使用 charlie（Editor）登录
2. 尝试分配 bob 审 alice 的论文 P1

**预期结果**:
- 返回错误: `409 Conflict: COI detected: Same affiliation as author`

---

### 测试用例 5: 自动推荐审稿人

**目标**: 基于关键词和领域匹配推荐审稿人

**前置条件**:
1. 创建多个 reviewer 并设置不同的 profile:
   - bob: fields=`AI,ML`, keywords=`deep learning,CNN`
   - 创建新用户 david (Reviewer): fields=`NLP,AI`, keywords=`transformer,BERT`
   - 创建新用户 eve (Reviewer): fields=`Robotics`, keywords=`control,planning`

2. alice 上传论文 P2:
   - fields: `AI,ML`
   - keywords: `deep learning,image recognition`

**步骤**:
1. 使用 charlie（Editor）登录
2. 选择 `5. Get Reviewer Recommendations`
3. 输入:
   - Paper ID: `P2`
   - Top K: `5`

**预期结果**:
- 返回排序后的推荐列表
- bob 应该排名最高（field 和 keyword 都匹配）
- david 次之（field 部分匹配）
- eve 排名最低（无匹配）
- 每个 reviewer 显示:
  - Relevance score
  - Active load
  - Final score
  - COI 状态

**示例输出**:
```
=== Top 5 Reviewer Recommendations for P2 ===

1. bob
   Relevance: 4.0
   Active Load: 0
   Final Score: 4.0
   [OK] No COI detected

2. david
   Relevance: 2.0
   Active Load: 0
   Final Score: 2.0
   [OK] No COI detected

3. eve
   Relevance: 0.0
   Active Load: 0
   Final Score: 0.0
   [OK] No COI detected
```

---

### 测试用例 6: 负载均衡

**目标**: 验证负载高的审稿人排名下降

**前置条件**:
1. 手动给 bob 分配多篇论文（达到 active_load = 3）
2. david 没有分配任何论文

**步骤**:
1. alice 上传新论文 P3（与 P2 类似的 fields/keywords）
2. charlie 查看推荐: `Get Reviewer Recommendations` for P3

**预期结果**:
- david 的 final_score 可能超过 bob（因为 bob 的 load penalty）
- 假设 lambda=0.5, bob 的 final_score = relevance - 0.5 * 3 = 4.0 - 1.5 = 2.5
- david 的 final_score = 2.0 - 0 = 2.0
- 如果 relevance 相近，load 低的会排前面

---

### 测试用例 7: 超过最大负载阈值

**目标**: 验证 MAX_ACTIVE 限制

**前置条件**:
1. 配置 max_active = 3（默认是 5）
2. bob 已有 3 个 pending assignments

**步骤**:
1. charlie 尝试手动分配 bob 到新论文 P4

**预期结果**:
- 返回错误: `409 Conflict: Reviewer has too many active assignments (3/3)`

---

### 测试用例 8: 自动分配审稿人

**目标**: 一键自动分配 N 个审稿人

**前置条件**:
1. 有足够的合格 reviewer（至少 3 个）
2. alice 上传论文 P5

**步骤**:
1. charlie（Editor）登录
2. 选择 `6. Auto Assign Reviewers`
3. 输入:
   - Paper ID: `P5`
   - Number: `2`

**预期结果**:
- 成功分配 2 个审稿人
- 返回分配的 reviewer 列表
- 论文状态更新为 `UNDER_REVIEW`
- Assignment 保存到 `/papers/P5/assignments.txt`
- 被分配的 reviewer 可以看到这篇论文

**示例输出**:
```
Successfully assigned 2 reviewers

Assigned reviewers:
- bob
- david
```

---

### 测试用例 9: 自动分配时 COI 过滤

**目标**: 自动分配会跳过有 COI 的审稿人

**前置条件**:
1. alice 上传论文 P6，并在 conflict_usernames 中指定 `bob`
2. 系统中有 bob, david, eve 三个 reviewer

**步骤**:
1. charlie 执行 `Auto Assign Reviewers` for P6, N=2

**预期结果**:
- bob 被 COI 过滤掉
- 只分配 david 和 eve
- 如果合格 reviewer 不足 N，返回错误提示

---

### 测试用例 10: 查看分配结果

**目标**: Reviewer 能看到分配给自己的论文

**步骤**:
1. 使用 bob 登录
2. 选择 `3. View Review Status` 或查看 assigned papers

**预期结果**:
- 能看到被分配的论文列表
- 包括 paper_id, 分配时间, 状态等

---

## 配置文件说明

### `/config/assignment.txt`
```
lambda=0.5
max_active=5
```

可以通过修改这些值来调整负载均衡策略。

---

## 数据文件位置（VFS 内）

- **Paper metadata**: `/papers/{paper_id}/metadata.txt`
  ```
  author=alice
  title=Deep Learning Paper
  status=SUBMITTED
  fields=AI,ML
  keywords=deep learning,CNN
  conflict_usernames=bob
  ```

- **Reviewer profile**: `/users/{username}/profile.txt`
  ```
  fields=AI,Machine Learning
  keywords=deep learning,neural networks
  affiliation=MIT
  coauthors=
  ```

- **Assignments**: `/papers/{paper_id}/assignments.txt`
  ```
  bob,1704067200,pending
  david,1704067200,pending
  ```

---

## 故障排查

### 1. 编译错误
- 确保包含了 `assignment_service.cpp` 在 CMakeLists.txt
- 检查所有头文件路径

### 2. 运行时错误
- 检查 VFS 是否正确挂载
- 查看服务器日志输出
- 确认 `/config`, `/users`, `/papers` 目录存在

### 3. 推荐结果为空
- 确认 reviewer 已设置 profile
- 确认 paper metadata 包含 fields/keywords
- 检查是否所有 reviewer 都被 COI 过滤

### 4. COI 检测不生效
- 确认 paper metadata 中 author 字段正确
- 确认 reviewer profile 已保存
- 检查 affiliation 字段是否匹配（大小写不敏感）

---

## 扩展测试

### 性能测试
- 创建 100+ papers 和 50+ reviewers
- 测试推荐算法性能
- 测试 load 计算效率

### 边界情况
- 没有任何 reviewer 有 profile
- 所有 reviewer 都被 COI 过滤
- paper 没有 fields/keywords
- 请求 Top-K 超过总 reviewer 数量

---

## 验收清单

- [ ] COI 检测：作者不能审自己
- [ ] COI 检测：相同机构被阻止
- [ ] COI 检测：黑名单生效
- [ ] 推荐排序：relevance 计算正确
- [ ] 推荐排序：显示每个 reviewer 的 score 和 load
- [ ] 负载均衡：load 高的排名下降
- [ ] 负载均衡：超过 MAX_ACTIVE 被排除
- [ ] 自动分配：成功写入 VFS assignments
- [ ] 自动分配：COI 过滤生效
- [ ] 数据持久化：profile 保存到 VFS
- [ ] 数据持久化：assignments 保存到 VFS
- [ ] 数据持久化：config 保存到 VFS

