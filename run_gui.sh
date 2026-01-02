#!/bin/bash

echo "=== 科研审稿系统 - GUI 启动脚本 ==="
echo ""

cd build

# 检查 GUI 可执行文件
if [ ! -f "./src/gui/review_gui" ]; then
    echo "❌ GUI 未编译，正在编译..."
    cmake .. -DBUILD_GUI=ON
    make review_gui -j4
    
    if [ $? -ne 0 ]; then
        echo "❌ GUI 编译失败"
        exit 1
    fi
fi

echo "✅ 启动 GUI 应用..."
echo ""
echo "提示："
echo "  - 服务器: 127.0.0.1"
echo "  - 端口: 8080"
echo "  - 测试账户: alice / password"
echo ""

# 启动 GUI
./src/gui/review_gui
