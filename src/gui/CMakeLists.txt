find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

set(GUI_SOURCES
    main.cpp
    main_window.cpp
    login_widget.cpp
    author_widget.cpp
)

set(GUI_HEADERS
    ${CMAKE_SOURCE_DIR}/include/gui/main_window.h
    ${CMAKE_SOURCE_DIR}/include/gui/login_widget.h
    ${CMAKE_SOURCE_DIR}/include/gui/author_widget.h
)

add_executable(review_gui
    ${GUI_SOURCES}
    ${GUI_HEADERS}
)

target_include_directories(review_gui PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(review_gui PRIVATE
    Qt6::Widgets
    Qt6::Network
    common
)
