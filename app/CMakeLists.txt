file(GLOB_RECURSE APPLICATION_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
    src/*.c
)

# Add Windows resource file (.rc) for Taskbar icon
if(WIN32)
    list(APPEND APPLICATION_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/app_icon.rc")
endif()

add_executable(application 
    ${APPLICATION_SOURCES}
)

target_include_directories(application
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(application
    PRIVATE
        olia
)

add_custom_command(TARGET application POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:olia>
        $<TARGET_FILE_DIR:application>
)

# Windows
if(WIN32)
    if(MINGW)
        target_link_options(application PRIVATE
            -static-libgcc
            -static-libstdc++
        )
    endif()

    # Hide console window for GUI application
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
        target_link_options(application PRIVATE
            -mwindows
        )
    endif()
endif()