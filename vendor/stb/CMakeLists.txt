cmake_minimum_required(VERSION 3.20)
project(STBLIB LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.c
)

add_library(stb STATIC
    ${SOURCES}
)

target_include_directories(stb
    PUBLIC
        include
)
