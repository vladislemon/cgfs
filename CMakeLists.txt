cmake_minimum_required(VERSION 3.24)
project(cgfs C)

set(CMAKE_C_STANDARD 99)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)

add_executable(cgfs ${SOURCES})

if (UNIX)
    target_link_libraries(cgfs GL)
endif (UNIX)
if (WIN32)
    target_link_libraries(cgfs WS2_32)
endif ()

add_custom_command(TARGET cgfs POST_BUILD COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}> ARGS $<TARGET_FILE:cgfs>)
