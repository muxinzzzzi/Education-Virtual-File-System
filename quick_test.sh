#!/bin/bash

cd "$(dirname "$0")"

echo "=========================================="
echo "  VFS 模块 A - 快速测试脚本"
echo "=========================================="
echo

# 编译
echo "[1/3] 编译 VFS 库..."
cd build
make -j4 > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ 编译成功"
else
    echo "❌ 编译失败"
    make
    exit 1
fi

echo

# 创建简单测试
echo "[2/3] 创建测试程序..."
cd ..
cat > /tmp/vfs_quick_test.cc << 'EOF'
#include "vfs.h"
#include "block_device.h"
#include <iostream>
#include <cassert>

int main() {
    std::cout << "🔧 初始化文件系统..." << std::endl;
    auto dev = MakeMemBlockDevice(1024, 4096);
    assert(Vfs::Mkfs(dev));
    auto vfs = Vfs::Mount(dev);
    assert(vfs != nullptr);
    std::cout << "✅ 文件系统初始化成功\n" << std::endl;
    
    std::cout << "📁 测试目录操作..." << std::endl;
    assert(vfs->Mkdir("/papers") == 0);
    assert(vfs->Mkdir("/reviews") == 0);
    assert(vfs->Mkdir("/papers/paper1") == 0);
    
    std::vector<std::string> names;
    assert(vfs->ListDir("/", &names) == 0);
    std::cout << "   根目录包含 " << names.size() << " 个条目:" << std::endl;
    for (const auto& n : names) {
        std::cout << "   - " << n << std::endl;
    }
    std::cout << "✅ 目录操作测试通过\n" << std::endl;
    
    std::cout << "📄 测试文件读写..." << std::endl;
    assert(vfs->CreateFile("/papers/test.txt") == 0);
    std::string data = "Hello, VFS! 这是一个测试文件。";
    assert(vfs->WriteFile("/papers/test.txt", 0, data) == 0);
    
    std::string content;
    assert(vfs->ReadFile("/papers/test.txt", 0, 100, &content) == 0);
    assert(content == data);
    std::cout << "   写入: " << data << std::endl;
    std::cout << "   读取: " << content << std::endl;
    std::cout << "✅ 文件读写测试通过\n" << std::endl;
    
    std::cout << "📦 测试大文件（跨块）..." << std::endl;
    assert(vfs->CreateFile("/papers/bigfile.bin") == 0);
    std::string bigdata(10000, 'A');
    assert(vfs->WriteFile("/papers/bigfile.bin", 0, bigdata) == 0);
    assert(vfs->ReadFile("/papers/bigfile.bin", 0, 10000, &content) == 0);
    assert(content.size() == 10000);
    std::cout << "   成功读写 " << bigdata.size() << " 字节" << std::endl;
    std::cout << "✅ 大文件测试通过\n" << std::endl;
    
    std::cout << "🗑️  测试删除操作..." << std::endl;
    assert(vfs->Unlink("/papers/test.txt") == 0);
    assert(vfs->Rmdir("/papers/paper1") == 0);
    assert(vfs->ListDir("/papers", &names) == 0);
    std::cout << "   删除后 /papers 包含 " << names.size() << " 个条目" << std::endl;
    std::cout << "✅ 删除操作测试通过\n" << std::endl;
    
    std::cout << "\n========================================" << std::endl;
    std::cout << "  ✅ 所有快速测试通过！" << std::endl;
    std::cout << "========================================" << std::endl;
    
    return 0;
}
EOF

g++ -std=c++17 -I vfs/include /tmp/vfs_quick_test.cc build/libvfs.a vfs/src/mem_block_device.cc -o /tmp/vfs_quick_test 2>&1
if [ $? -ne 0 ]; then
    echo "❌ 测试程序编译失败"
    exit 1
fi

echo

# 运行测试
echo "[3/3] 运行测试..."
echo "=========================================="
/tmp/vfs_quick_test
TEST_RESULT=$?

echo
if [ $TEST_RESULT -eq 0 ]; then
    echo "🎉 VFS 模块 A 功能正常！"
else
    echo "❌ 测试失败，请检查日志"
fi

# 清理
rm -f /tmp/vfs_quick_test /tmp/vfs_quick_test.cc

exit $TEST_RESULT

