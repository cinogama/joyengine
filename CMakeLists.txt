# JoyECS
#
cmake_minimum_required (VERSION 3.8)

project ("JoyECS")
set(CMAKE_CXX_STANDARD 17)

if(MSVC) 
    add_compile_options("$<$<CXX_COMPILER_ID:MSVC>:/source-charset:utf-8>")
    add_compile_options("$<$<C_COMPILER_ID:MSVC>:/source-charset:utf-8>")
endif()

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/build")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/build")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glfw" FORCE)
add_subdirectory("3rd/glfw")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glew" FORCE)
add_subdirectory("3rd/glew/build/cmake")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic woolang" FORCE)
add_subdirectory("3rd/woolang")


if(WIN32)
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /E /I)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\woolang\\include\\wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y)
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\\" /Y /E /I)
else()
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND mkdir -p \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/include/\" && cp \"${CMAKE_CURRENT_SOURCE_DIR}/include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/\" -r)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/3rd/woolang/include/wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/\")
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND mkdir -p \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/\" && cp \"${CMAKE_CURRENT_SOURCE_DIR}/builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/\" -r)
endif()

add_subdirectory("src")

include_directories("include")
include_directories("3rd/woolang/include")

file(GLOB_RECURSE jedriver_src driver/*.cpp)
add_executable(jedriver ${jedriver_src})

add_dependencies(jedriver COPY_WOOLANG_FILES)
add_dependencies(COPY_WOOLANG_FILES COPY_ENGINE_FILES)
add_dependencies(jedriver COPY_BUILTIN_FILES)

target_link_libraries(jedriver joyecs)