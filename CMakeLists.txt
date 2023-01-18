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

# WHY DYNAMIC? FREEALUT IS LGPL, JOYENGINE WILL OPENSOURCE WITH APACHE2.0
set(OPENAL_INCLUDE_DIR "3rd/openalsdk/include")
set(OPENAL_LIBRARY "3rd/openalsdk/libs")

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic freealut" FORCE)
add_subdirectory("3rd/freealut")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glfw" FORCE)
add_subdirectory("3rd/glfw")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glew" FORCE)
add_subdirectory("3rd/glew/build/cmake")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static box2d" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Donot built box2d test cases." FORCE)
add_subdirectory("3rd/box2d")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic woolang" FORCE)
add_subdirectory("3rd/woolang")


if(WIN32)
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\\" /Y /S /I /D)
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /S /I /D)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\woolang\\include\\wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /I /D)
    add_custom_target(COPY_CMAKE_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\cmake\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\cmake\" /Y /S /I /D)
else()
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin\" -r)
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/include\" -r)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/3rd/woolang/include/wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/include\")
    add_custom_target(COPY_CMAKE_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/3rd/cmake\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/cmake\" -r)
endif()

add_subdirectory("src")

include_directories("include")
include_directories("3rd/woolang/include")

file(GLOB_RECURSE jedriver_src driver/*.cpp)
add_executable(jedriver ${jedriver_src})

add_dependencies(COPY_CMAKE_FILES COPY_BUILTIN_FILES)
add_dependencies(COPY_ENGINE_FILES COPY_BUILTIN_FILES)
add_dependencies(COPY_WOOLANG_FILES COPY_ENGINE_FILES)
add_dependencies(jedriver COPY_WOOLANG_FILES)

target_link_libraries(jedriver joyecs)