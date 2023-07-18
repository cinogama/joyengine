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

set(ENV{OPENALDIR} "${CMAKE_CURRENT_SOURCE_DIR}/3rd/openalsdk")
include(FindOpenAL)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glfw" FORCE)
add_subdirectory("3rd/glfw")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glew" FORCE)
add_subdirectory("3rd/glew/build/cmake")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static box2d" FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Donot built box2d test cases." FORCE)
add_subdirectory("3rd/box2d")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic woolang" FORCE)
add_subdirectory("3rd/woolang")

add_custom_target(INSTALL_PKG COMMAND baozi install WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})

if(WIN32)
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\\" /Y /S /I /D)
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /S /I /D)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\woolang\\include\\wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /I /D)
    add_custom_target(COPY_CMAKE_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\cmake\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\cmake\" /Y /S /I /D)
    add_custom_target(COPY_RCEDIT64_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\rcedit-x64.exe\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\" /Y /I /D)
    add_custom_target(COPY_RCEDIT32_FILES 
        COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\rcedit-x86.exe\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\" /Y /I /D)
else()
    add_custom_target(COPY_BUILTIN_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin\" -r)
    add_custom_target(COPY_ENGINE_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/include\" -r)
    add_custom_target(COPY_WOOLANG_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/3rd/woolang/include/wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/include\")
    add_custom_target(COPY_CMAKE_FILES 
        COMMAND cp \"${CMAKE_CURRENT_SOURCE_DIR}/3rd/cmake\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/builtin/cmake\" -r)
    add_custom_target(COPY_RCEDIT64_FILES 
        COMMAND echo "COPY_RCEDIT64_FILES is only used for win32")
    add_custom_target(COPY_RCEDIT32_FILES 
        COMMAND xcopy "COPY_RCEDIT64_FILES is only used for win32")
endif()

add_subdirectory("src")

include_directories("include")
include_directories("3rd/woolang/include")

file(GLOB_RECURSE jedriver_src driver/*.cpp)
add_executable(jedriver ${jedriver_src})

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set_target_properties(jedriver PROPERTIES RELEASE_POSTFIX "")
	set_target_properties(jedriver PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(jedriver PROPERTIES RELEASE_POSTFIX "32")
	set_target_properties(jedriver PROPERTIES DEBUG_POSTFIX "32_debug")
else()
	message(FATAL_ERROR "Unknown platform")
endif()

add_dependencies(COPY_ENGINE_FILES COPY_BUILTIN_FILES)
add_dependencies(COPY_WOOLANG_FILES COPY_ENGINE_FILES)
add_dependencies(COPY_CMAKE_FILES COPY_WOOLANG_FILES)
add_dependencies(COPY_RCEDIT64_FILES COPY_CMAKE_FILES)
add_dependencies(COPY_RCEDIT32_FILES COPY_RCEDIT64_FILES)
add_dependencies(INSTALL_PKG COPY_RCEDIT32_FILES)
add_dependencies(jedriver INSTALL_PKG)

target_link_libraries(jedriver joyecs)