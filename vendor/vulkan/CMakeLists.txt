cmake_minimum_required(VERSION 3.10)
project(vulkan_sdk)

# 1. Define the Interface Library
add_library(vulkan INTERFACE)
add_library(Vulkan::Vulkan ALIAS vulkan)

# 2. Set Include Directories
target_include_directories(vulkan INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

# 3. Find and Link the Vulkan Loader Library
# On Windows, this is usually vulkan-1.lib (or .dll.a for MinGW)
find_library(VULKAN_LOADER
    NAMES vulkan-1 vulkan
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib"
    NO_DEFAULT_PATH
)

if(VULKAN_LOADER)
    target_link_libraries(vulkan INTERFACE "${VULKAN_LOADER}")
    message(STATUS "Vulkan Loader found: ${VULKAN_LOADER}")
else()
    message(FATAL_ERROR "Could not find vulkan-1 library in ${CMAKE_CURRENT_SOURCE_DIR}/lib")
endif()

# 4. Optional: Handle Volk (since it's in your include tree)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include/Volk/volk.h")
    add_library(volk INTERFACE)
    add_library(Vulkan::Volk ALIAS volk)
    target_include_directories(volk INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include/Volk")
    # Volk usually needs the vulkan headers but handles loading itself
    target_link_libraries(volk INTERFACE vulkan)
endif()

# 5. Optional: Add Shaderc / Glslang (Common for engines)
# These are often used for runtime shader compilation
find_library(SHADERC_LIB NAMES shaderc_shared shaderc PATHS "${CMAKE_CURRENT_SOURCE_DIR}/lib" NO_DEFAULT_PATH)
if(SHADERC_LIB)
    add_library(shaderc INTERFACE)
    target_link_libraries(shaderc INTERFACE "${SHADERC_LIB}")
    target_include_directories(shaderc INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endif()