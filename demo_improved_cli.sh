#!/bin/bash

# 演示改进后的CLI客户端

echo "======================================"
echo "  科研审稿系统 - CLI改进演示"
echo "======================================"
echo ""
echo "主要改进:"
echo "  ✓ 彩色输出和图标提示"
echo "  ✓ 智能上下文记忆（记住最近操作的论文ID等）"
echo "  ✓ 友好的输入提示和默认值"
echo "  ✓ 美观的菜单布局"
echo "  ✓ 操作确认和进度提示"
echo "  ✓ 按回车继续，防止信息一闪而过"
echo ""
echo "现在启动系统进行演示..."
echo ""

cd "$(dirname "$0")"

# 检查构建
if [ ! -f "build/review_server" ] || [ ! -f "build/review_client" ]; then
    echo "正在编译项目..."
    ./compile.sh
fi

# 启动服务器
echo "启动服务器..."
cd build
./review_server review_system.img > /tmp/review_server.log 2>&1 &
SERVER_PID=$!
echo "服务器PID: $SERVER_PID"

sleep 2

echo ""
echo "======================================"
echo "  服务器已启动，请使用客户端连接"
echo "======================================"
echo ""
echo "测试账号:"
echo "  作者:   alice / password123"
echo "  审稿人: bob / password123"
echo "  编辑:   editor1 / password123"
echo "  管理员: admin / admin123"
echo ""
echo "启动客户端..."
echo ""

# 启动客户端
./review_client localhost 8080

# 清理
echo ""
echo "关闭服务器..."
kill $SERVER_PID 2>/dev/null
wait $SERVER_PID 2>/dev/null

echo "演示结束！"

