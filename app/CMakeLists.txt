file(GLOB_RECURSE APPLICATION_SOURCES
    CONFIGURE_DEPENDS
    src/*.cpp
    src/*.c
)

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