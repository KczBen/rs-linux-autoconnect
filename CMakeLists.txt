cmake_minimum_required(VERSION 3.10)
project(rs-linux-autoconnect C)

set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_FLAGS "-m32")

add_library(rs-linux-autoconnect SHARED main.c)

target_link_libraries(rs-linux-autoconnect dl jack)

set_target_properties(rs-linux-autoconnect PROPERTIES
    OUTPUT_NAME "rsshim"
)
