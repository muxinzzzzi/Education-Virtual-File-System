cmake_minimum_required(VERSION 3.16)
project(os_vfs CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(vfs STATIC
  vfs/src/mem_block_device.cc
  vfs/src/vfs.cc
)

target_include_directories(vfs PUBLIC vfs/include)

add_executable(test_vfs
  vfs/tests/test_vfs.cc
)

target_link_libraries(test_vfs PRIVATE vfs)