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
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic rscene" FORCE)
add_subdirectory("3rd/restorablescene")


add_subdirectory("src")
add_subdirectory("driver")