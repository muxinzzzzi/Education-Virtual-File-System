#!/bin/bash

# 文件系统核心特性演示脚本

clear

cat << "EOF"
╔════════════════════════════════════════════════════════════════════╗
║                                                                    ║
║          教育虚拟文件系统 - 核心特性演示                          ║
║       Educational Virtual File System - Feature Demonstration     ║
║                                                                    ║
╚════════════════════════════════════════════════════════════════════╝

EOF

echo -e "\033[1;36m本演示将展示以下核心特性:\033[0m"
echo ""
echo "  📦 1. 超级块 (Superblock) 结构"
echo "       - 文件系统元数据"
echo "       - 块分配统计"
echo "       - Inode管理信息"
echo ""
echo "  🗂️  2. 文件系统布局"
echo "       - 超级块区域"
echo "       - Inode表"
echo "       - 空闲块位图 (Free Bitmap)"
echo "       - 数据块区域"
echo ""
echo "  ⚡ 3. LRU缓存机制"
echo "       - 可配置容量 (本demo: 256 blocks)"
echo "       - 缓存命中/未命中统计"
echo "       - 缓存驱逐机制"
echo "       - 性能优化效果"
echo ""
echo "  📁 4. 多级目录结构"
echo "       - 目录创建和嵌套"
echo "       - 路径解析"
echo "       - 目录遍历"
echo ""
echo "  📄 5. 文件操作"
echo "       - 文件创建和删除"
echo "       - 文件读写"
echo "       - 文件描述符管理"
echo ""
echo "  💾 6. 备份与恢复"
echo "       - 创建系统快照"
echo "       - 列出所有备份"
echo "       - 版本化管理"
echo ""
echo -e "\033[1;33m准备启动演示工具...\033[0m"
echo ""
read -p "按回车键开始 >> " dummy

cd "$(dirname "$0")"

# 检查是否已编译
if [ ! -f "build/src/tools/fs_demo" ]; then
    echo -e "\033[1;33m正在编译演示工具...\033[0m"
    cd build
    cmake .. > /dev/null 2>&1
    make fs_demo -j4
    cd ..
    echo -e "\033[1;32m✓ 编译完成\033[0m"
    echo ""
fi

# 清理旧的演示文件
rm -f build/fs_demo.img build/fs_demo.img.* 2>/dev/null

# 运行演示
echo -e "\033[1;36m════════════════════════════════════════════════════════════════════\033[0m"
echo ""

cd build/src/tools
./fs_demo

cd ../../..

echo ""
echo -e "\033[1;36m════════════════════════════════════════════════════════════════════\033[0m"
echo ""
echo -e "\033[1;32m✓ 演示完成！\033[0m"
echo ""
echo -e "\033[1;36m关键要点回顾:\033[0m"
echo ""
echo "  1. ✅ 超级块包含完整的文件系统元数据"
echo "  2. ✅ Inode表管理所有文件和目录的元信息"
echo "  3. ✅ Free Bitmap高效管理数据块分配"
echo "  4. ✅ LRU缓存显著提升访问性能"
echo "  5. ✅ 支持完整的多级目录结构"
echo "  6. ✅ 文件系统提供统一API供上层调用"
echo "  7. ✅ 备份功能确保数据安全"
echo ""
echo -e "\033[1;33m演示文件位置: build/fs_demo.img\033[0m"
echo -e "\033[1;33m你可以查看该文件以验证文件系统的持久化存储\033[0m"
echo ""

