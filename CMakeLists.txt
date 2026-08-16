cmake_minimum_required(VERSION 3.20)
project(Olia LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# -------------------------------------------------------------------
# 1. Find Shader Compiler (glslc)
# -------------------------------------------------------------------
find_program(GLSLC_EXE 
    NAMES glslc 
    PATHS "${CMAKE_CURRENT_SOURCE_DIR}/vendor/vulkan/bin"
    NO_DEFAULT_PATH
)

if(NOT GLSLC_EXE)
    message(FATAL_ERROR "glslc not found! Expected at vendor/vulkan/bin/glslc")
endif()

# -------------------------------------------------------------------
# 2. Shader Compilation Logic
# -------------------------------------------------------------------
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shaders")
set(SHADER_BINARY_DIR "${SHADER_SOURCE_DIR}/compiled")

file(MAKE_DIRECTORY ${SHADER_BINARY_DIR})

# List your shaders explicitly here
set(SHADERS
    # "main.vert"
    # "main.frag"
)

set(SPV_FILES "")

foreach(SHADER_NAME ${SHADERS})
    set(SHADER_PATH "${SHADER_SOURCE_DIR}/${SHADER_NAME}")
    set(SPV_PATH "${SHADER_BINARY_DIR}/${SHADER_NAME}.spv")
    
    add_custom_command(
        OUTPUT ${SPV_PATH}
        COMMAND ${GLSLC_EXE} ${SHADER_PATH} -o ${SPV_PATH}
        DEPENDS ${SHADER_PATH}
        COMMENT "Compiling shader: ${SHADER_NAME}"
    )
    list(APPEND SPV_FILES ${SPV_PATH})
endforeach()

add_custom_target(CompileShaders ALL DEPENDS ${SPV_FILES})

# -------------------------------------------------------------------
# 3. Subdirectories
# -------------------------------------------------------------------
add_subdirectory(vendor)
add_subdirectory(engine)
add_subdirectory(app)

# -------------------------------------------------------------------
# 4. Asset Synchronization
# -------------------------------------------------------------------
# This copies the entire assets folder (including the newly compiled shaders)
# to the directory where application.exe is built.
add_custom_target(CopyAssets
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/assets"
    "$<TARGET_FILE_DIR:application>/assets"
    COMMENT "Copying assets to build directory..."
)

# Set dependencies: 
# Compile Shaders -> then Copy Assets -> then build Application
add_dependencies(CopyAssets CompileShaders)
add_dependencies(application CopyAssets)