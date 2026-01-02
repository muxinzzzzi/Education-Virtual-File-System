#!/bin/bash

# 科研审稿系统 - 快速使用指南

echo "=== 科研审稿系统构建与测试 ==="
echo ""

# 编译项目
echo "1. 编译项目..."
cd /Users/serena.c/Education-Virtual-File-System
mkdir -p build
cd build
cmake .. && make -j4

if [ $? -ne 0 ]; then
    echo "❌ 编译失败"
    exit 1
fi

echo "✅ 编译成功！"
echo ""

# 运行VFS测试
echo "2. 运行文件系统测试..."
./tests/test_vfs

echo ""
echo "=== 使用指南 ==="
echo ""
echo "启动服务器："
echo "  cd build"
echo "  ./src/server/review_server [port] [filesystem_image]"
echo "  示例: ./src/server/review_server 8080 review.img"
echo ""
echo "启动客户端（新终端）："
echo "  cd build"
echo "  ./src/client/review_client [host] [port]"
echo "  示例: ./src/client/review_client 127.0.0.1 8080"
echo ""
echo "默认测试账户："
echo "  管理员: admin / admin123"
echo "  作者: alice / password"
echo "  审稿人: bob / password"
echo "  编辑: charlie / password"
echo ""
