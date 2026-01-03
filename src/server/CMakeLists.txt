add_library(server STATIC
    auth_manager.cpp
    review_server.cpp
    assignment_service.cpp
)

target_include_directories(server PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(server PUBLIC
    filesystem
    common
    OpenSSL::SSL
    OpenSSL::Crypto
    Threads::Threads
)

# Server executable
add_executable(review_server
    server_main.cpp
)

target_link_libraries(review_server PRIVATE
    server
)
