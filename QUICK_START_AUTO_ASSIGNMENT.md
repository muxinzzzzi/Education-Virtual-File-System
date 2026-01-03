# 快速开始：自动审稿人分配功能

## 5 分钟快速体验

### 步骤 1: 编译项目

```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System
./demo_auto_assignment.sh
```

这会自动编译项目并显示使用说明。

### 步骤 2: 启动服务器（终端 1）

```bash
cd build
./src/server/review_server 8080 test_auto.img
```

等待看到：
```
Server started on port 8080
```

### 步骤 3: 启动客户端（终端 2）

```bash
cd build
./src/client/review_client 127.0.0.1 8080
```

---

## 快速测试流程

### 测试 1: 设置审稿人 Profile (1 分钟)

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

### 测试 2: 上传论文 (1 分钟)

```
1. 登录: alice / password
2. 选择: 1 (Upload Paper)
3. 输入:
   Title: Deep Learning Research
   File: /tmp/test_paper.pdf
4. 记录 paper_id (例如 P1)
5. 选择: 5 (Logout)
```

**注意**: 如果 `/tmp/test_paper.pdf` 不存在，运行：
```bash
echo "Test paper content" > /tmp/test_paper.pdf
```

### 测试 3: COI 检测 (30 秒)

```
1. 登录: charlie / password (Editor)
2. 选择: 1 (Assign Reviewer Manual)
3. 输入:
   Paper ID: P1
   Reviewer: alice
4. 预期结果: ❌ 409 Conflict: COI detected: Reviewer is the paper author
```

### 测试 4: 自动推荐 (30 秒)

```
1. 继续使用 charlie 登录
2. 选择: 5 (Get Reviewer Recommendations)
3. 输入:
   Paper ID: P1
   Top K: 5
4. 查看推荐列表（显示 score, load, COI 状态）
```

### 测试 5: 自动分配 (30 秒)

```
1. 继续使用 charlie 登录
2. 选择: 6 (Auto Assign Reviewers)
3. 输入:
   Paper ID: P1
   Number: 2
4. 预期结果: ✅ Successfully assigned 2 reviewers
```

### 测试 6: 验证分配 (30 秒)

```
1. 选择: 7 (Logout)
2. 登录: bob / password
3. 选择: 3 (View Review Status)
4. 输入 Paper ID: P1
5. 应该能看到这篇论文已分配给 bob
```

---

## 常见问题

### Q1: 编译失败？
```bash
# 清理重新编译
cd build
rm -rf *
cmake ..
make -j4
```

### Q2: 服务器启动失败？
```bash
# 删除旧的镜像文件
rm -f build/*.img
# 重新启动
./src/server/review_server 8080 test_auto.img
```

### Q3: 推荐列表为空？
- 确保 reviewer 已设置 profile（步骤 1）
- 确保 paper 有 fields/keywords（需要手动修改 VFS 或等待客户端支持）

### Q4: COI 检测不生效？
- 确认 paper metadata 中有正确的 author 字段
- 确认 reviewer profile 已保存

### Q5: 如何修改配置？
配置文件在 VFS 内部 `/config/assignment.txt`：
```
lambda=0.5        # 负载惩罚权重
max_active=5      # 最大活跃分配数
```

目前需要手动修改 VFS 文件或通过代码初始化。

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

---

## 核心功能验证清单

- [x] ✅ 编译成功
- [ ] ✅ 服务器启动
- [ ] ✅ 客户端连接
- [ ] ✅ Profile 设置
- [ ] ✅ Profile 查看
- [ ] ✅ 论文上传
- [ ] ✅ COI 检测（作者）
- [ ] ✅ 自动推荐
- [ ] ✅ 自动分配
- [ ] ✅ 分配验证

---

## 下一步

完成快速测试后，可以：

1. 阅读详细文档：`AUTO_ASSIGNMENT_IMPLEMENTATION.md`
2. 查看完整测试用例：`test_auto_assignment.md`
3. 探索更多功能：负载均衡、COI 规则等
4. 扩展功能：添加更多 COI 规则、改进推荐算法

---

## 技术支持

如有问题，请检查：

1. **编译日志**: 查看 make 输出
2. **服务器日志**: 查看终端 1 的输出
3. **VFS 文件**: 检查 `/users/`, `/papers/`, `/config/` 目录
4. **权限**: 确认用户角色正确（Editor 才能推荐/分配）

---



