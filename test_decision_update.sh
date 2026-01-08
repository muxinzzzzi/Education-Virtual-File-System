#!/bin/bash

# 测试决定更新是否生效的脚本

cd /Users/muxin/Desktop/Education-Virtual-File-System/build

echo "=== 1. 停止所有旧服务器 ==="
killall review_server 2>/dev/null
sleep 2

echo ""
echo "=== 2. 清理文件系统（重新开始）==="
rm -f review_system.img review_system.img.*

echo ""
echo "=== 3. 启动服务器 ==="
./src/server/review_server &
SERVER_PID=$!
sleep 3

echo ""
echo "=== 4. 现在请手动测试：==="
echo ""
echo "终端1（客户端-作者alice）："
echo "  ./src/client/review_client 127.0.0.1 8080"
echo "  登录: alice / password123"
echo "  选择 [1] 上传论文"
echo ""
echo "终端2（客户端-编辑editor）："
echo "  ./src/client/review_client 127.0.0.1 8080"
echo "  登录: editor / password123"
echo "  选择 [6] 自动分配审稿人 (给P1分配1个审稿人)"
echo "  选择 [2] 做出决定 (输入P1，选择1=accept)"
echo "  选择 [3] 查看待处理论文 (输入0，看P1是否还在列表中)"
echo ""
echo "终端3（客户端-作者alice）："
echo "  选择 [2] 查看论文状态 (输入0或P1，看状态是否变为Accepted)"
echo ""
echo "预期结果："
echo "  - 编辑做出accept决定后，编辑的待处理列表中P1应该显示为'Accepted ✓'"
echo "  - 作者查看论文状态时，P1应该显示为'Accepted ✓'"
echo ""
echo "服务器PID: $SERVER_PID"
echo "测试完成后，执行: kill $SERVER_PID"
echo ""

# 等待用户测试
read -p "按回车键停止服务器..."
kill $SERVER_PID 2>/dev/null
echo "服务器已停止"

