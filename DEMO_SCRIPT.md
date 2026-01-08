# 四终端并发演示操作脚本

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
登录：
```
alice
password123
```

### 终端3：编辑（editor1）
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/client/review_client 127.0.0.1 8080
```
登录：
```
editor1
password123
```

### 终端4：审稿人（bob）
```bash
cd /Users/muxin/Desktop/Education-Virtual-File-System/build
./src/client/review_client 127.0.0.1 8080
```
登录：
```
bob
password123
```

---

## 📋 演示流程

### 第1步：作者上传论文

**终端2（alice）**
```
1
AI Research Paper on Deep Learning
/Users/muxin/Desktop/Education-Virtual-File-System/nips1.pdf
single
AI,Machine Learning,Deep Learning
neural networks,transformers,attention
tim

```
*(记住显示的paper_id，比如P001)*

---

### 第2步：编辑查看待处理论文

**终端3（editor1）**
```
3

```

---

### 第3步：编辑查看审稿进度

**终端3（editor1）**
```
4
P001

```

---

### 第4步：编辑获取审稿人推荐

**终端3（editor1）**
```
5
P001
5

```

---

### 第5步：编辑手动分配审稿人

**终端3（editor1）**
```
1
P001
bob
R1


```

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

### 第7步：审稿人查看个人资料

**终端4（bob）**
```
5

```

---

### 第8步：审稿人下载论文

**终端4（bob）**
```
1
P001
downloaded_paper.pdf

```

---

### 第9步：审稿人查看审稿状态

**终端4（bob）**
```
3
P001

```

---

### 第10步：审稿人提交审稿意见

**终端4（bob）**
```
2
P001
R1
/Users/muxin/Desktop/Education-Virtual-File-System/data/review_file/bobP1.txt

```

---

### 第11步：作者查看论文状态

**终端2（alice）**
```
2
P001

```

---

### 第12步：作者下载审稿意见

**终端2（alice）**
```
4
P001


```

---

### 第13步：编辑再次查看审稿进度

**终端3（editor1）**
```
4
P001

```

---

### 第14步：编辑做出最终决定

**终端3（editor1）**
```
2
P001
accept
y

```

---

### 第15步：作者查看最终决定

**终端2（alice）**
```
2


```

---

### 第16步：作者提交修订版（如果需要）

**终端2（alice）**
```
3
P001
/Users/muxin/Desktop/Education-Virtual-File-System/nips1.pdf

```

---

### 第17步：编辑自动分配审稿人（演示智能匹配）

**终端2（alice）** - 先上传另一篇论文
```
1
Computer Vision Research
/Users/muxin/Desktop/Education-Virtual-File-System/2010.11929v2.pdf
double
Computer Vision,AI
CNN,object detection


```
*(记住新的paper_id，比如P002)*

**终端3（editor1）** - 自动分配
```
6
P002
3
y

```

---

## 🎯 展示的所有功能

### 作者功能 ✅
- [x] 上传论文（带元数据）
- [x] 查看论文状态
- [x] 提交修订版
- [x] 下载审稿意见

### 审稿人功能 ✅
- [x] 设置个人资料（研究领域/关键词）
- [x] 查看个人资料
- [x] 下载待审论文
- [x] 查看审稿状态
- [x] 提交审稿意见

### 编辑功能 ✅
- [x] 查看待处理论文
- [x] 查看审稿进度
- [x] 手动分配审稿人
- [x] 获取审稿人推荐（智能匹配）
- [x] 自动分配审稿人
- [x] 做出最终决定

### 核心特性展示 ✅
- [x] 多客户端并发连接
- [x] 实时数据同步
- [x] 用户认证和权限控制
- [x] 智能上下文记忆（paper_id自动填充）
- [x] 文件上传下载
- [x] 智能审稿人匹配算法
- [x] 完整的审稿流程

---

## 💡 操作提示

1. **空行表示直接按回车**（使用默认值或记住的值）
2. **paper_id** 在每次操作时会被系统记住，可以直接回车
3. **文件路径** 需要根据实际情况调整
4. **每步操作后按回车继续**
5. **可以在不同终端间切换展示实时性**

---

## 🎬 录制顺序建议

1. **开场**：展示4个终端布局
2. **启动**：先启动服务器，再启动3个客户端并登录
3. **演示**：按照上述步骤1-17依次操作
4. **重点**：在步骤11、13、15时强调"实时看到其他用户的操作"
5. **收尾**：展示服务器日志，说明并发处理能力

---

## ⚡ 关键展示点

在演示时特别指出：

1. **步骤1→2**：alice上传论文后，editor1立即能看到
2. **步骤5→8**：editor1分配后，bob立即能下载
3. **步骤10→11**：bob提交审稿后，alice立即能看到
4. **步骤14→15**：editor1决定后，alice立即看到结果
5. **步骤17**：自动匹配算法根据论文和审稿人的领域/关键词智能推荐

这展示了：
- ✅ 实时并发处理
- ✅ 完整业务流程
- ✅ 智能匹配算法
- ✅ 用户友好的CLI界面

