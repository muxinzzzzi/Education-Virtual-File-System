# 自动审稿人分配

## 实现的功能

### 1. COI (Conflict of Interest) 检测

实现了以下 COI 规则：

- 禁止 reviewer == author
- 禁止相同 affiliation（机构）
- 支持作者指定 conflict_usernames 黑名单
- 支持 coauthors 关系检测（预留）

**实现位置**: `AssignmentService::check_coi()`

**应用场景**:
- 手动分配审稿人时自动检查
- 自动推荐时标记 COI 状态
- 自动分配时过滤 COI 审稿人

### 2. 基于关键词/领域的自动推荐

**相关性打分公式**:
```
relevance = 2.0 × |Field_paper ∩ Field_reviewer| + 1.0 × |Keyword_paper ∩ Keyword_reviewer|
```

- 字段匹配权重更高（2.0）
- 关键词匹配权重次之（1.0）

**实现位置**: `AssignmentService::compute_relevance()`

### 3. 负载均衡

**最终评分公式**:
```
final_score = relevance - λ × active_assignments
```

**配置参数**:
- `lambda`: 负载惩罚权重（默认 0.5）
- `max_active`: 最大活跃分配数（默认 5）

**特点**:
- 动态计算每个 reviewer 的 active_load
- 超过 max_active 的 reviewer 自动排除
- 配置保存在 VFS `/config/assignment.txt`

**实现位置**: `AssignmentService::get_active_load()`, `recommend_reviewers()`

### 4. 自动推荐与分配

**推荐功能** (`GET_REVIEWER_RECOMMENDATIONS`):
- 返回 Top-K 候选审稿人
- 显示每个候选人的：
  - Relevance score
  - Active load
  - Final score
  - COI 状态和原因

**自动分配功能** (`AUTO_ASSIGN_REVIEWERS`):
- 自动选择前 N 个合格审稿人
- 过滤 COI 冲突
- 过滤负载过高的审稿人
- 写入 assignments 到 VFS
- 更新论文状态为 UNDER_REVIEW

---

## 快速测试流程

编译，创建镜像，并创建测试文件
```bash
./demo_auto_assignment.sh
```

### 测试 1: 设置审稿人 Profile 

```
1. 登录: bob / password
2. 选择: 4 (Set My Profile)
3. 输入:
   Fields: AI,Machine Learning
   Keywords: deep learning,CNN
   Affiliation: MIT
4. 选择: 5 (View My Profile) 验证
5. 选择: 6 (Logout)
```
- Profile 成功保存到 VFS `/users/bob/profile.txt`
- 查看时能看到刚才输入的信息

### 测试 2: 上传论文
```
1. 登录: alice / password
2. 选择: 1 (Upload Paper)
3. 输入:
   Title: Deep Learning Research
   File: /tmp/test_paper.pdf
4. 记录 paper_id (例如 P1)
5. 选择: 5 (Logout)
```
- Metadata 保存到 `/papers/P1/metadata.txt`
**注意**: 如果 `/tmp/test_paper.pdf` 不存在，运行：
```bash
echo "Test paper content" > /tmp/test_paper.pdf
```

### 测试 3: COI 验证

**前置条件**: 
1. 先为 alice 设置 profile（需要先创建 alice 的 reviewer profile）
2. alice 和 bob 都设置 affiliation 为 `MIT`
```
1. 登录: charlie / password (Editor)
2. 选择: 1 (Assign Reviewer Manual)
3. 输入:
   Paper ID: P1
   Reviewer: alice
4. 预期结果: 
   - 返回错误: `409 Conflict: COI detected: Reviewer is the paper author`
   - 分配失败
```

### 测试 4: 自动推荐

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

### 测试 5: 自动分配
**目标**: 一键自动分配 N 个审稿人

**前置条件**:
1. 有足够的合格 reviewer（至少 3 个）
2. alice 上传论文 P5
```
1. 继续使用 charlie 登录
2. 选择: 6 (Auto Assign Reviewers)
3. 输入:
   Paper ID: P1
   Number: 2
4. 预期结果: Successfully assigned 2 reviewers
```

**验证分配**
```
1. 选择: 7 (Logout)
2. 登录: bob / password
3. 选择: 3 (View Review Status)
4. 输入 Paper ID: P1
5. 应该能看到这篇论文已分配给 bob
```

### 测试 6: 负载均衡

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

### 测试 7: 负载阈值

**目标**: 验证 MAX_ACTIVE 限制

**前置条件**:
1. 配置 max_active = 3（默认是 5）
2. bob 已有 3 个 pending assignments

**步骤**:
1. charlie 尝试手动分配 bob 到新论文 P4

**预期结果**:
- 返回错误: `409 Conflict: Reviewer has too many active assignments (3/3)`

---

## 进阶测试

### 创建更多测试用户

```bash
# 在客户端中
1. 登录: admin / admin123
2. 选择: 1 (Create User)
3. 创建:
   - david / password / reviewer
   - eve / password / reviewer
```

### 设置不同的 Profile

为每个 reviewer 设置不同的 fields/keywords，测试推荐排序：

- bob: AI, ML → deep learning, CNN
- david: NLP, AI → transformer, BERT
- eve: Robotics → control, planning

### 测试负载均衡

1. 创建多篇论文 (P1, P2, P3)
2. 手动分配 bob 到 P1, P2
3. 推荐 P3 时，bob 的 final_score 会降低


## 代码文件

### 服务端

1. **`include/server/assignment_service.h`**
   - AssignmentService 类定义
   - 数据结构：PaperMeta, ReviewerProfile, Assignment, RecommendationResult
   - 配置结构：AssignmentConfig

2. **`src/server/assignment_service.cpp`**
   - COI 检测引擎
   - 相关性评分算法
   - 负载管理
   - 自动推荐与分配逻辑
   - VFS 数据持久化

### 协议

3. **修改 `include/common/protocol.h`**
   - 新增命令：
     - `SET_REVIEWER_PROFILE`
     - `GET_REVIEWER_PROFILE`
     - `GET_REVIEWER_RECOMMENDATIONS`
     - `AUTO_ASSIGN_REVIEWERS`

### 服务端集成

4. **修改 `include/server/review_server.h`**
   - 添加 `assignment_service_` 成员
   - 新增 handler 方法声明

5. **修改 `src/server/review_server.cpp`**
   - 初始化 AssignmentService
   - 实现新的 handler 方法
   - 修改 `handle_assign_reviewer` 添加 COI 检查
   - 修改 `handle_upload_paper` 支持 fields/keywords

6. **修改 `src/server/auth_manager.cpp`**
   - 添加新命令的权限检查

### 客户端

7. **修改 `include/client/review_client.h`**
   - 新增方法声明

8. **修改 `src/client/review_client.cpp`**
   - Editor 菜单增加自动推荐和自动分配选项
   - Reviewer 菜单增加 profile 管理选项
   - 实现新方法

### 构建

9. **修改 `src/server/CMakeLists.txt`**
   - 添加 `assignment_service.cpp` 到编译列表

---

## VFS 数据结构

### 1. Paper Metadata (`/papers/{paper_id}/metadata.txt`)

```
author=alice
title=Deep Learning Research
status=SUBMITTED
fields=AI,Machine Learning,Computer Vision
keywords=deep learning,CNN,image recognition
conflict_usernames=bob,charlie
```

### 2. Reviewer Profile (`/users/{username}/profile.txt`)

```
fields=AI,Machine Learning,NLP
keywords=deep learning,transformer,BERT
affiliation=MIT
coauthors=john,jane
```

### 3. Assignments (`/papers/{paper_id}/assignments.txt`)

```
bob,1704067200,pending
david,1704067200,submitted
eve,1704067300,pending
```

格式: `reviewer,timestamp,state`

### 4. Configuration (`/config/assignment.txt`)

```
lambda=0.5
max_active=5
```

---

## 网络协议

### SET_REVIEWER_PROFILE

**请求**:
```
Command: SET_REVIEWER_PROFILE
Params:
  fields: AI,Machine Learning
  keywords: deep learning,neural networks
  affiliation: MIT
```

**响应**:
```
Status: 200 OK
Message: Profile updated
```

### GET_REVIEWER_PROFILE

**请求**:
```
Command: GET_REVIEWER_PROFILE
Params:
  username: bob (optional, defaults to current user)
```

**响应**:
```
Status: 200 OK
Body:
Username: bob
Fields: AI, Machine Learning
Keywords: deep learning, neural networks
Affiliation: MIT
```

### GET_REVIEWER_RECOMMENDATIONS

**请求**:
```
Command: GET_REVIEWER_RECOMMENDATIONS
Params:
  paper_id: P1
  k: 5
```

**响应**:
```
Status: 200 OK
Body:
=== Top 5 Reviewer Recommendations for P1 ===

1. bob
   Relevance: 4.0
   Active Load: 1
   Final Score: 3.5
   [OK] No COI detected

2. david
   Relevance: 2.0
   Active Load: 0
   Final Score: 2.0
   [OK] No COI detected

3. alice
   Relevance: 5.0
   Active Load: 0
   Final Score: 5.0
   [BLOCKED] Reviewer is the paper author
```

### AUTO_ASSIGN_REVIEWERS

**请求**:
```
Command: AUTO_ASSIGN_REVIEWERS
Params:
  paper_id: P1
  n: 2
```

**响应**:
```
Status: 200 OK
Body:
Successfully assigned 2 reviewers

Assigned reviewers:
- bob
- david
```

---

## 权限控制

| 命令 | Author | Reviewer | Editor | Admin |
|------|--------|----------|--------|-------|
| SET_REVIEWER_PROFILE | ❌ | ✅ | ❌ | ✅ |
| GET_REVIEWER_PROFILE | ❌ | ✅ | ❌ | ✅ |
| GET_REVIEWER_RECOMMENDATIONS | ❌ | ❌ | ✅ | ✅ |
| AUTO_ASSIGN_REVIEWERS | ❌ | ❌ | ✅ | ✅ |

---

## 关键算法

### COI 检测算法

```cpp
COIResult check_coi(const PaperMeta &paper, const ReviewerProfile &reviewer) {
  // 1. Check reviewer == author
  if (reviewer.username == paper.author) {
    return {true, "Reviewer is the paper author"};
  }
  
  // 2. Check author's blacklist
  if (paper.conflict_usernames contains reviewer.username) {
    return {true, "Reviewer in author's conflict list"};
  }
  
  // 3. Check same affiliation
  if (author.affiliation == reviewer.affiliation) {
    return {true, "Same affiliation as author"};
  }
  
  // 4. Check coauthor relationship
  if (reviewer.coauthors contains paper.author) {
    return {true, "Co-author relationship"};
  }
  
  return {false, ""};
}
```

### 推荐排序算法

```cpp
vector<RecommendationResult> recommend_reviewers(paper_id, top_k) {
  results = [];
  
  for each reviewer in all_reviewers:
    // Compute relevance
    field_match = |paper.fields ∩ reviewer.fields|
    keyword_match = |paper.keywords ∩ reviewer.keywords|
    relevance = 2.0 * field_match + 1.0 * keyword_match
    
    // Check COI
    coi = check_coi(paper, reviewer)
    
    // Get active load
    active_load = count_pending_assignments(reviewer)
    
    // Check max_active threshold
    if active_load >= max_active:
      coi.blocked = true
      coi.reason = "Exceeds max active assignments"
    
    // Compute final score
    final_score = relevance - lambda * active_load
    
    results.append({reviewer, relevance, active_load, final_score, coi})
  
  // Sort by final_score descending
  sort(results, by=final_score, descending=true)
  
  return results[0:top_k]
}
```

### 自动分配算法

```cpp
AssignResult auto_assign(paper_id, num_reviewers) {
  // Get recommendations
  recommendations = recommend_reviewers(paper_id, num_reviewers * 2)
  
  // Filter out COI-blocked
  valid = filter(recommendations, where=!coi_blocked)
  
  if valid.size() < num_reviewers:
    return {false, "Not enough valid reviewers"}
  
  // Take top N
  selected = valid[0:num_reviewers]
  
  // Create assignments
  assignments = []
  for reviewer in selected:
    assignments.append({
      paper_id: paper_id,
      reviewer: reviewer.username,
      assigned_at: now(),
      state: "pending"
    })
  
  // Save to VFS
  save_assignments(paper_id, assignments)
  
  // Update paper status
  update_paper_status(paper_id, "UNDER_REVIEW")
  
  return {true, "Success", selected}
}
```

---

## 性能考虑

### 当前实现
- **负载计算**: O(P × A)，P=论文数，A=每篇论文的分配数
- **推荐计算**: O(R × (F + K))，R=审稿人数，F=字段数，K=关键词数
- **COI 检测**: O(1) ~ O(C)，C=冲突列表长度

### 优化方向（未来）
1. 缓存 reviewer 的 active_load
2. 建立倒排索引（field/keyword -> reviewers）
3. 增量更新推荐结果
4. 批量分配优化