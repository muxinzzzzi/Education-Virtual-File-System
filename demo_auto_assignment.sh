#!/bin/bash

# 自动审稿人分配功能演示脚本
# 此脚本展示如何测试新功能

echo "======================================"
echo "自动审稿人分配功能演示"
echo "======================================"
echo ""

echo "1. 编译项目..."
cd "$(dirname "$0")"
mkdir -p build
cd build
cmake .. > /dev/null 2>&1
make review_server review_client -j4 > /dev/null 2>&1

if [ $? -ne 0 ]; then
    echo "❌ 编译失败，请检查代码"
    exit 1
fi

echo "✅ 编译成功"
echo ""

echo "2. 清理旧的测试镜像..."
rm -f test_auto_assignment.img
echo "✅ 清理完成"
echo ""

echo "======================================"
echo "启动说明"
echo "======================================"
echo ""
echo "请按以下步骤手动测试："
echo ""
echo "终端 1 - 启动服务器:"
echo "  cd $(pwd)"
echo "  ./src/server/review_server 8080 test_auto_assignment.img"
echo ""
echo "终端 2 - 启动客户端:"
echo "  cd $(pwd)"
echo "  ./src/client/review_client 127.0.0.1 8080"
echo ""
echo "======================================"
echo "测试场景"
echo "======================================"
echo ""
echo "场景 1: 设置 Reviewer Profile"
echo "  1. 登录: bob / password (Reviewer)"
echo "  2. 选择: 4. Set My Profile"
echo "  3. 输入 fields: AI,Machine Learning"
echo "  4. 输入 keywords: deep learning,neural networks"
echo "  5. 输入 affiliation: MIT"
echo "  6. 选择: 5. View My Profile 验证"
echo ""

echo "场景 2: 上传论文（带 fields/keywords）"
echo "  1. 登录: alice / password (Author)"
echo "  2. 选择: 1. Upload Paper"
echo "  3. 输入 title: Deep Learning Research"
echo "  4. 输入文件路径: /tmp/test.pdf (创建一个测试文件)"
echo "  5. 记录返回的 paper_id (如 P1)"
echo ""

echo "场景 3: COI 检测 - 作者不能审自己的论文"
echo "  1. 登录: charlie / password (Editor)"
echo "  2. 选择: 1. Assign Reviewer (Manual)"
echo "  3. 输入 Paper ID: P1"
echo "  4. 输入 Reviewer: alice (论文作者)"
echo "  5. 预期: 返回 COI 错误"
echo ""

echo "场景 4: 自动推荐审稿人"
echo "  1. 登录: charlie / password (Editor)"
echo "  2. 选择: 5. Get Reviewer Recommendations"
echo "  3. 输入 Paper ID: P1"
echo "  4. 输入 Top K: 5"
echo "  5. 查看推荐列表（包含 score, load, COI 状态）"
echo ""

echo "场景 5: 自动分配审稿人"
echo "  1. 登录: charlie / password (Editor)"
echo "  2. 选择: 6. Auto Assign Reviewers"
echo "  3. 输入 Paper ID: P1"
echo "  4. 输入 Number: 2"
echo "  5. 查看分配结果"
echo ""

echo "场景 6: 负载均衡测试"
echo "  1. 重复场景 2 创建多篇论文 (P2, P3, P4)"
echo "  2. 手动分配 bob 到多篇论文"
echo "  3. 再次推荐时，bob 的 final_score 会因 load 降低"
echo ""

echo "======================================"
echo "创建测试文件"
echo "======================================"
echo ""

# 创建测试论文文件
echo "Creating test paper file..."
echo "This is a test paper for the peer review system." > /tmp/test_paper.pdf
echo "Title: Deep Learning for Image Classification" >> /tmp/test_paper.pdf
echo "Abstract: This paper presents..." >> /tmp/test_paper.pdf
echo "✅ Test file created at /tmp/test_paper.pdf"
echo ""

echo "======================================"
echo "数据文件位置（VFS 内部）"
echo "======================================"
echo ""
echo "- Reviewer Profile: /users/{username}/profile.txt"
echo "- Paper Metadata: /papers/{paper_id}/metadata.txt"
echo "- Assignments: /papers/{paper_id}/assignments.txt"
echo "- Config: /config/assignment.txt"
echo ""

echo "======================================"
echo "默认配置"
echo "======================================"
echo ""
echo "- lambda (负载惩罚权重): 0.5"
echo "- max_active (最大活跃分配): 5"
echo ""

echo "======================================"
echo "准备就绪！"
echo "======================================"
echo ""
echo "请在两个终端中分别运行服务器和客户端"
echo "详细测试步骤请参考: test_auto_assignment.md"
echo ""

