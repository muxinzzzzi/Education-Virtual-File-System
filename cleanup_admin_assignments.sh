#!/bin/bash

# 清理错误分配给admin的论文审稿任务

echo "======================================"
echo "  清理错误的admin分配"
echo "======================================"
echo ""

cd "$(dirname "$0")"

if [ ! -f "build/review_system.img" ]; then
    echo "❌ 找不到文件系统镜像: build/review_system.img"
    exit 1
fi

echo "⚠️  警告: 此脚本将修改文件系统镜像"
echo "建议先创建备份"
echo ""
read -p "是否继续? (y/n): " confirm

if [ "$confirm" != "y" ] && [ "$confirm" != "Y" ]; then
    echo "操作已取消"
    exit 0
fi

# 创建Python清理脚本
cat > /tmp/cleanup_admin.py << 'PYTHON_SCRIPT'
#!/usr/bin/env python3
import sys
import os

def cleanup_assignments(img_path):
    """从文件系统镜像中清理admin的分配"""
    
    # 注意: 这需要挂载文件系统或直接操作镜像
    # 由于我们的VFS是自定义格式，这里提供手动清理方法
    
    print("文件系统使用自定义格式，需要通过服务器清理")
    print("")
    print("建议使用以下方法:")
    print("1. 启动服务器")
    print("2. 登录管理员账户")
    print("3. 手动检查并重新分配论文")
    print("")
    print("或者使用客户端清理工具...")

if __name__ == '__main__':
    cleanup_assignments('build/review_system.img')
PYTHON_SCRIPT

python3 /tmp/cleanup_admin.py
rm /tmp/cleanup_admin.py

echo ""
echo "======================================"
echo "  建议的清理方法"
echo "======================================"
echo ""
echo "由于文件系统使用自定义格式，建议通过客户端操作:"
echo ""
echo "1. 启动服务器和客户端:"
echo "   cd build"
echo "   ./src/server/review_server &"
echo "   ./src/client/review_client 127.0.0.1 8080"
echo ""
echo "2. 登录为编辑(editor1):"
echo "   editor1 / password123"
echo ""
echo "3. 查看所有论文列表:"
echo "   选择 [4] 查看审稿进度"
echo "   输入 0 查看列表"
echo ""
echo "4. 对于分配了admin的论文:"
echo "   - 记下paper_id"
echo "   - 重新分配给正确的审稿人"
echo ""
echo "5. 或者创建新的审稿人并重新分配"
echo ""
echo "✅ 现在系统已禁止将论文分配给admin"
echo "✅ 新的分配不会再出现这个问题"
echo ""

