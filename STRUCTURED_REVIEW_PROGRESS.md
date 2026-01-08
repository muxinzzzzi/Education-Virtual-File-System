# 结构化审稿系统实现进度

## ✅ 已完成（服务器端）

### 1. 数据结构定义
- 创建了 `include/server/review_data.h`
- 定义了 `StructuredReview` 结构，包含：
  - 总评（summary）
  - 优点（strengths）
  - 缺点（weaknesses）
  - 问题/建议（questions）
  - 评分（rating 1-5）
  - 置信度（confidence 1-5）
  - 状态（draft/submitted）
  - 时间戳

### 2. 服务器端实现
- ✅ 重写 `handle_submit_review` - 接收结构化数据，保存为JSON
- ✅ 新增 `handle_save_review_draft` - 保存草稿
- ✅ 新增 `handle_get_review_draft` - 读取已保存的审稿意见
- ✅ 添加新命令：`SAVE_REVIEW_DRAFT`, `GET_REVIEW_DRAFT`
- ✅ 更新协议映射和权限配置
- ✅ 编译通过

### 3. 审稿文件格式变化
- **旧格式**：`/papers/P1/rounds/R1/reviews/bob.txt` (纯文本文件)
- **新格式**：`/papers/P1/rounds/R1/reviews/bob.json` (JSON结构化数据)

## ⏳ 待完成（客户端）

### 4. 客户端表单式审稿界面
需要修改 `src/client/review_client.cpp` 的 `submit_review()` 函数：

**当前实现**：
- 提示输入本地文件路径
- 读取文件并上传

**需要改为**：
1. 首先尝试加载已保存的草稿（调用GET_REVIEW_DRAFT）
2. 显示交互式表单：
   ```
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    📝 审稿论文: P1
   ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
   
   [必填] 请输入总评 (Summary):
   > 这篇论文提出了一个新颖的方法...
   
   [选填] 请输入优点 (Strengths):
   > 1. 方法创新
   > 2. 实验充分
   > (输入空行结束)
   
   [选填] 请输入缺点 (Weaknesses):
   > 1. 写作需要改进
   > (输入空行结束)
   
   [选填] 请输入问题/建议 (Questions):
   > 作者是否考虑过...
   > (输入空行结束)
   
   [必填] 请输入评分 (1-5):
     1 - Strong Reject
     2 - Weak Reject
     3 - Borderline
     4 - Weak Accept
     5 - Strong Accept
   > 4
   
   [必填] 请输入置信度 (1-5):
     1 - Very Low
     2 - Low
     3 - Medium
     4 - High
     5 - Very High
   > 4
   
   选择操作:
     [1] 💾 保存草稿 (可稍后继续编辑)
     [2] ✅ 提交审稿意见 (不可再修改)
     [3] ❌ 取消
   > 
   ```

3. 根据用户选择：
   - 选1：调用SAVE_REVIEW_DRAFT保存草稿
   - 选2：调用SUBMIT_REVIEW提交最终版
   - 选3：取消操作

### 5. 编辑查看审稿意见
需要修改编辑的"查看审稿进度"功能，显示结构化的审稿数据：

**当前显示**：（旧的handle_view_review_progress只显示论文状态）

**需要改为**：
- 读取所有审稿人的.json文件
- 解析并显示结构化内容：
  ```
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    论文 P1 的审稿意见汇总
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  
  [审稿人1] bob
  评分: 4 - Weak Accept | 置信度: 4/5 | 状态: ✓ 已提交
  
  【总评】
  这篇论文提出了一个新颖的方法...
  
  【优点】
  1. 方法创新
  2. 实验充分
  
  【缺点】
  1. 写作需要改进
  
  【问题/建议】
  作者是否考虑过...
  
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  
  [审稿人2] charlie
  评分: 5 - Strong Accept | 置信度: 5/5 | 状态: ✓ 已提交
  
  【总评】
  优秀的工作...
  
  ...
  ```

## 🔧 下一步操作

### 1. 先测试服务器端（验证BUG修复）
由于刚才修复了决定更新的BUG，应该先测试：

```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
killall review_server
rm -f review_system.img review_system.img.*
./src/server/review_server
```

在另一个终端测试完整流程（alice上传→editor分配→**editor做决定**），验证：
- 决定是否正确保存为ACCEPTED
- 列表是否正确显示状态

### 2. 实现客户端表单（大工作量）
这需要大量修改 `review_client.cpp`，包括：
- 修改 `submit_review()` 函数
- 添加多行文本输入辅助函数
- 修改 `view_review_progress()` 或相关函数以显示结构化审稿意见

### 3. 全面测试
- 审稿人：保存草稿 → 加载草稿 → 继续编辑 → 提交
- 编辑：查看审稿意见 → 做出决定
- 作者：查看论文状态（应显示决定）

## ⚠️ 兼容性说明

新系统保存为`.json`文件，旧系统是`.txt`文件。建议：
1. **删除旧数据重新开始** (推荐): `rm -f build/review_system.img*`
2. 或实现向后兼容（读取旧.txt文件并转换为结构化数据）

## 📦 相关文件

- `include/server/review_data.h` - 数据结构定义
- `include/common/protocol.h` - 新命令定义
- `src/server/review_server.cpp` - 服务器处理逻辑
- `src/server/auth_manager.cpp` - 权限配置
- `src/client/review_client.cpp` - **待修改**

## 📝 注意事项

1. 所有审稿文件现在是JSON格式，可以用任何JSON解析器读取
2. 草稿可以被覆盖，提交后会标记时间戳
3. rating和confidence必须是1-5的整数
4. summary和rating是必填项，其他字段可选

