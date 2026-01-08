# Tools for filesystem demonstration

add_executable(fs_demo fs_demo.cpp)
target_link_libraries(fs_demo PRIVATE filesystem common)
target_include_directories(fs_demo PRIVATE ${PROJECT_SOURCE_DIR}/include)

