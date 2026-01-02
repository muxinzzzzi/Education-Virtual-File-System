add_library(client_lib STATIC
    review_client.cpp
)

target_include_directories(client_lib PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(client_lib PUBLIC
    common
)

# Client executable
add_executable(review_client
    client_main.cpp
)

target_link_libraries(review_client PRIVATE
    client_lib
)
