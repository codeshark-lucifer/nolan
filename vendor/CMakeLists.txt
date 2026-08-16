set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_STATIC ON CACHE BOOL "" FORCE)

set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)

set(FT_DISABLE_ZLIB ON CACHE BOOL "" FORCE)
set(FT_DISABLE_BZIP2 ON CACHE BOOL "" FORCE)
set(FT_DISABLE_PNG ON CACHE BOOL "" FORCE)
set(FT_DISABLE_HARFBUZZ ON CACHE BOOL "" FORCE)

find_package(OpenSSL QUIET)

add_subdirectory(freetype)
add_subdirectory(harfbuzz)
add_subdirectory(SDL)
add_subdirectory(vulkan)
add_subdirectory(glm)
add_subdirectory(stb)

# Add minizip if it has its own CMakeLists.txt, or expose its source directory
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/minizip/CMakeLists.txt")
    add_subdirectory(minizip)
endif()

# ImGui Source Files
set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
set(IMGUI_SOURCES
    ${IMGUI_DIR}/imgui.cpp
    ${IMGUI_DIR}/imgui_draw.cpp
    ${IMGUI_DIR}/imgui_tables.cpp
    ${IMGUI_DIR}/imgui_widgets.cpp
    ${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp
    ${IMGUI_DIR}/backends/imgui_impl_vulkan.cpp
)

add_library(imgui STATIC ${IMGUI_SOURCES})
target_include_directories(imgui PUBLIC 
    ${IMGUI_DIR} 
    ${IMGUI_DIR}/backends
    ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include
    ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include
)
target_link_libraries(imgui PUBLIC SDL3::SDL3-static Vulkan::Vulkan)

add_library(vendor INTERFACE)

target_include_directories(vendor
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}/SDL/include
        ${CMAKE_CURRENT_SOURCE_DIR}/vulkan/include
        ${CMAKE_CURRENT_SOURCE_DIR}/glm
        ${CMAKE_CURRENT_SOURCE_DIR}/stb/include
        ${CMAKE_CURRENT_SOURCE_DIR}/freetype/include
        ${CMAKE_CURRENT_SOURCE_DIR}/harfbuzz/src
        ${CMAKE_CURRENT_SOURCE_DIR}/httplib
        ${CMAKE_CURRENT_SOURCE_DIR}/nlohmann/include     
        ${CMAKE_CURRENT_SOURCE_DIR}/minizip
        ${IMGUI_DIR}/backends
        ${IMGUI_DIR}
)

if(OPENSSL_FOUND)
    target_compile_definitions(vendor INTERFACE CPPHTTPLIB_OPENSSL_SUPPORT)
    target_link_libraries(vendor INTERFACE OpenSSL::SSL OpenSSL::Crypto)
endif()

target_link_libraries(vendor
    INTERFACE
    SDL3::SDL3-static
    Vulkan::Vulkan
    glm
    stb
    freetype
    harfbuzz
    imgui
    urlmon
)