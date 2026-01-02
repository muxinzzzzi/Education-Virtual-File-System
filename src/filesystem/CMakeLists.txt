add_library(filesystem STATIC
    bitmap.cpp
    lru_cache.cpp
    vfs.cpp
    vfs_file_ops.cpp
    vfs_io.cpp
)

target_include_directories(filesystem PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(filesystem PUBLIC
    Threads::Threads
)
