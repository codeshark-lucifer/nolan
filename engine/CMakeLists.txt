cmake_minimum_required(VERSION 3.20)

project(OliaEngine)

file(GLOB_RECURSE ENGINE_SOURCES
    CONFIGURE_DEPENDS
    src/vulkan/*.cpp
    src/*.cpp
    src/*.c
)

add_library(olia SHARED ${ENGINE_SOURCES})

target_compile_definitions(olia
    PRIVATE OLIA_BUILD
)

target_include_directories(olia
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(olia
    PUBLIC
        vendor
)