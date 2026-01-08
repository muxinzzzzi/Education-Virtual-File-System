# 四终端并发演示操作脚本（含列表功能）

## 🚀 终端启动命令

### 终端1：服务器
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/server/review_server
```

### 终端2：作者（alice）
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/client/review_client 127.0.0.1 8080
```
```
alice
password123
```

### 终端3：编辑（editor1）
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/client/review_client 127.0.0.1 8080
```
```
editor1
password123
```

### 终端4：审稿人（bob）
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/client/review_client 127.0.0.1 8080
```
```
bob
password123
```

---

## 📋 完整演示流程

### 第1步：作者上传论文

**终端2（alice）**
```
1
AI Research on Deep Learning
/Users/muxin/Desktop/Education-Virtual-File-System/nips1.pdf
single
AI,Machine Learning,Deep Learning
neural networks,transformers
tim

```
*(记住paper_id: P1或P001)*

---

### 第2步：作者查看论文列表 ⭐ 新功能

**终端2（alice）**
```
2
0

```
*(系统显示论文列表)*

```
P001

```
*(查看P001详细状态)*

---

### 第3步：编辑查看所有论文 ⭐ 新功能

**终端3（editor1）**
```
4
0

```
*(系统显示所有论文列表，包括作者、审稿人、状态)*

---

### 第4步：编辑分配审稿人

**终端3（editor1）**
```
1
P001
bob
R1


```

---

### 第5步：审稿人查看待审列表 ⭐ 新功能

**终端4（bob）**
```
3
0

```
*(系统显示分配给bob的所有论文)*

```
P001

```
*(查看P001的详细审稿信息)*

---

### 第6步：审稿人设置个人资料

**终端4（bob）**
```
4
AI,Machine Learning,Computer Vision
deep learning,neural networks,CNN
Stanford University

```

---

### 第7步：审稿人下载论文

**终端4（bob）**
```
1


```
*(直接回车使用记住的P001)*

---

### 第8步：审稿人提交审稿意见

**终端4（bob）**
```
2


/Users/muxin/Desktop/Education-Virtual-File-System/data/review_file/bobP1.txt

```

---

### 第9步：作者查看论文列表（看到状态更新）

**终端2（alice）**
```
2
0

```
*(列表中看到P001状态变为"Under Review")*

---

### 第10步：作者下载审稿意见

**终端2（alice）**
```
4



```

---

### 第11步：编辑查看进度

**终端3（editor1）**
```
4
0

```
*(看到P001有bob的审稿)*

```
P001

```

---

### 第12步：编辑做出决定

**终端3（editor1）**
```
2

accept
y

```

---

### 第13步：作者再次查看列表（看到最终决定）⭐

**终端2（alice）**
```
2
0

```
*(列表中看到P001状态变为"Accepted ✓")*

---

### 第14步：作者上传第二篇论文

**终端2（alice）**
```
1
Computer Vision Research
/Users/muxin/Desktop/Education-Virtual-File-System/2010.11929v2.pdf
double
Computer Vision,AI
CNN,object detection


```

---

### 第15步：编辑查看更新后的论文列表 ⭐

**终端3（editor1）**
```
4
0

```
*(看到新增的P2或P002)*

---

### 第16步：编辑获取审稿人推荐

**终端3（editor1）**
```
5
P002
5

```

---

### 第17步：编辑自动分配审稿人

**终端3（editor1）**
```
6

3
y

```
*(使用记住的P002，自动分配3个审稿人)*

---

### 第18步：审稿人查看更新的待审列表 ⭐

**终端4（bob）**
```
3
0

```
*(看到新分配的P002)*

---

## 🎯 展示重点

### 列表功能的优势 ⭐

1. **第2步**: 作者看到自己的所有论文概览
2. **第3步**: 编辑快速了解系统中所有论文
3. **第5步**: 审稿人知道自己有哪些审稿任务
4. **第9步**: 实时看到状态更新（Under Review）
5. **第13步**: 实时看到最终决定（Accepted ✓）
6. **第15步**: 新论文立即出现在列表中
7. **第18步**: 新分配的任务立即可见

### 实时并发演示

通过四终端演示，展示：
- ✅ alice上传 → editor立即在列表中看到
- ✅ editor分配 → bob立即在待审列表看到
- ✅ bob提交审稿 → alice在列表中看到状态更新
- ✅ editor做决定 → alice在列表中看到最终结果

---

## 📊 展示的所有功能

### 作者功能 (4个)
- [x] 上传论文
- [x] **查看论文列表** ⭐ 新功能
- [x] 查看论文详细状态
- [x] 提交修订版
- [x] 下载审稿意见

### 审稿人功能 (5个)
- [x] **查看待审论文列表** ⭐ 新功能
- [x] 查看论文详情
- [x] 设置个人资料
- [x] 下载论文
- [x] 提交审稿意见

### 编辑功能 (6个)
- [x] **查看所有论文列表** ⭐ 新功能
- [x] 查看详细审稿进度
- [x] 手动分配审稿人
- [x] 获取智能推荐
- [x] 自动分配审稿人
- [x] 做出最终决定

### 管理员功能 (已修复)
- [x] 创建用户（支持数字和名称）
- [x] **删除用户** ⭐ 新功能
- [x] 查看用户列表
- [x] 系统管理功能

---

## 🎬 录制建议

### 演示顺序

1. **开场** (1分钟)
   - 展示四终端布局
   - 说明架构

2. **启动系统** (1分钟)
   - 启动服务器
   - 登录四个角色

3. **核心演示** (12-15分钟)
   - 按步骤1-18操作
   - **重点突出列表功能**（第2,3,5,9,13,15,18步）
   - 强调实时性

4. **收尾** (1分钟)
   - 总结展示的特性
   - 展示服务器日志

---

## ⚡ 关键演示点

在演示时特别指出：

1. ✅ **不需要记paper_id** - 输入0就能看列表
2. ✅ **实时同步** - 其他用户操作立即反映在列表中
3. ✅ **完整信息** - 列表显示关键信息一目了然
4. ✅ **便捷操作** - 从列表选择比手动输入ID方便
5. ✅ **智能记忆** - 系统记住最近操作，更加流畅

---

## 🎉 总结

新增的列表功能使系统：
- 📊 **更直观** - 一眼看到所有相关论文
- ⚡ **更快速** - 不用记ID
- 🔄 **实时性** - 立即看到更新
- 👍 **更易用** - 降低使用门槛

**准备好开始录制了！** 🎥

