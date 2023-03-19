cmake_minimum_required(VERSION 3.24)
project(cgfs C)

set(CMAKE_C_STANDARD 99)
set(SHADER_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/shaders)

find_package(Vulkan COMPONENTS glslc)
find_program(glslc_executable NAMES glslc HINTS Vulkan::glslc)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.c)
file(GLOB_RECURSE SHADERS CONFIGURE_DEPENDS src/shaders/*.*)

add_custom_command(
        COMMAND ${CMAKE_COMMAND} -E make_directory ${SHADER_BINARY_DIR}
        OUTPUT ${SHADER_BINARY_DIR}
        COMMENT "Creating directory ${SHADER_BINARY_DIR}"
        VERBATIM
)

foreach(source IN LISTS SHADERS)
    get_filename_component(FILENAME ${source} NAME)
    add_custom_command(
            COMMAND ${glslc_executable} -o ${SHADER_BINARY_DIR}/${FILENAME}.spv ${source}
            OUTPUT ${SHADER_BINARY_DIR}/${FILENAME}.spv
            DEPENDS ${source}
            COMMENT "Compiling shader ${FILENAME}"
            VERBATIM
    )
    list(APPEND SPV_SHADERS ${SHADER_BINARY_DIR}/${FILENAME}.spv)
endforeach()

add_executable(cgfs ${SOURCES})

if (WIN32)
    target_link_libraries(cgfs ws2_32 vulkan-1)
elseif (UNIX)
    target_link_libraries(cgfs xcb vulkan)
endif ()

add_custom_command(TARGET cgfs POST_BUILD COMMAND $<$<CONFIG:release>:${CMAKE_STRIP}> ARGS $<TARGET_FILE:cgfs>)

add_custom_target(shaders_dir DEPENDS ${SHADER_BINARY_DIR})
add_custom_target(shaders DEPENDS ${SPV_SHADERS})
add_dependencies(shaders shaders_dir)
add_dependencies(cgfs shaders)
