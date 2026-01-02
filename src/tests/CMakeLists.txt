add_executable(test_vfs test_vfs.cpp)

target_link_libraries(test_vfs PRIVATE
    filesystem
)
