# JoyECS
#
cmake_minimum_required (VERSION 3.16)

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

option(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD  "Install pkgs by baozi when building."  ON)
option(JE4_COPYING_BUILTIN_FILES            "Copy builtin files to build."          ON)
option(JE4_EXECUTE_BUILD_SCRIPT             "Copy liscences & gles-sdk for win32."  ON)

option(JE4_ENABLE_DX11_GAPI         "Enable DX11 Graphic api"           OFF)
option(JE4_ENABLE_OPENGL330_GAPI    "Enable OpenGL 3.3 Graphic API"     OFF)
option(JE4_ENABLE_OPENGLES300_GAPI  "Enable OpenGLES 3.0 Graphic API"   OFF)
option(JE4_ENABLE_VULKAN130_GAPI    "Enable Vulkan 1.3 Graphic API"     OFF)
option(JE4_ENABLE_METAL_GAPI        "Enable Metal Graphic API"          OFF)


# Cannot use gles & gl at same time because of ImGUI cannot use two lib at same time.
if (JE4_ENABLE_OPENGL330_GAPI AND JE4_ENABLE_OPENGLES300_GAPI)
    message(FATAL_ERROR "Cannot use OpenGL3.3 and OpenGLES3.0 at same time.")
endif()

# openal-soft will be compiled to dynamic-libs
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared opengal-soft" FORCE)
add_subdirectory("3rd/openal-soft")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static box2d" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "Donot built box2d unit-test." FORCE)
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "Donot built box2d test cases." FORCE)
add_subdirectory("3rd/box2d")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic woolang" FORCE)
add_subdirectory("3rd/woolang")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static assimp" FORCE)
add_subdirectory("3rd/assimp")
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glslangc" FORCE)
add_subdirectory("3rd/glslang")
# NOTE: Donot need spirv-cross. we will not generate hlsl/glsl/msl from spir-v.
# set(SPIRV_CROSS_EXCEPTIONS_TO_ASSERTIONS ON CACHE BOOL "Build static spirv-cross" FORCE)
# set(SPIRV_CROSS_SHARED OFF CACHE BOOL "Build static spirv-cross" FORCE)
# set(SPIRV_CROSS_STATIC ON CACHE BOOL "Build static spirv-cross" FORCE)
# add_subdirectory("3rd/spirv-cross")

if (JE4_ENABLE_OPENGL330_GAPI)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glew" FORCE)
    add_subdirectory("3rd/glew/build/cmake")
endif()
if (JE4_ENABLE_DX11_GAPI 
    OR JE4_ENABLE_OPENGL330_GAPI 
    OR JE4_ENABLE_OPENGLES300_GAPI 
    OR JE4_ENABLE_VULKAN130_GAPI)

    if (NOT ANDROID)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static glfw" FORCE)
        add_subdirectory("3rd/glfw")
    endif()
endif()

if(MSVC) 
    ## Fuck the MSVC, you did a great job that if we disable the exception, 
    ## the compiler boomed! FUCK!
    
    # add_definitions(-D_HAS_EXCEPTIONS=0)
    # if(CMAKE_CXX_FLAGS MATCHES "/EHsc ")
    #     string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # endif()
    # if(CMAKE_CXX_FLAGS MATCHES "/EHsc$")
    #     string(REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    # endif()
else()
    add_compile_options(-fno-exceptions)
endif()

if (JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD)
    set (WO_CMAKE_BUILD_TYPE "${CMAKE_BUILD_TYPE}")
    string (TOUPPER ${WO_CMAKE_BUILD_TYPE} WO_CMAKE_BUILD_TYPE_UPPER_CASE)

    if (${WO_CMAKE_BUILD_TYPE_UPPER_CASE} STREQUAL "DEBUG")
        add_custom_target(INSTALL_PKG COMMAND baozi install -nonative -profile debug WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    else()
        add_custom_target(INSTALL_PKG COMMAND baozi install -nonative WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
else()
    add_custom_target(INSTALL_PKG COMMAND echo "JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD is disabled.")
endif()

if (JE4_EXECUTE_BUILD_SCRIPT)
    add_custom_target(EXECUTE_BUILD_SCRIPT COMMAND
        woodriver ./script/build.wo WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    add_dependencies(EXECUTE_BUILD_SCRIPT INSTALL_PKG)
    add_dependencies(EXECUTE_BUILD_SCRIPT woodriver)
else()
    add_custom_target(EXECUTE_BUILD_SCRIPT COMMAND 
        echo "JE4_EXECUTE_BUILD_SCRIPT is disabled.")
endif()

if (JE4_COPYING_BUILTIN_FILES)
    if(${CMAKE_HOST_SYSTEM_NAME} MATCHES "Windows")
        add_custom_target(COPY_BUILTIN_FILES 
            COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\builtin\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\\" /Y /S /I /D)
        add_custom_target(COPY_ENGINE_FILES 
            COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\include\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /S /I /D)
        add_custom_target(COPY_WOOLANG_FILES 
            COMMAND xcopy \"${CMAKE_CURRENT_SOURCE_DIR}\\3rd\\woolang\\include\\wo.h\" \"${CMAKE_RUNTIME_OUTPUT_DIRECTORY}\\builtin\\include\" /Y /I /D)
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
        add_custom_target(COPY_RCEDIT64_FILES 
            COMMAND echo "COPY_RCEDIT64_FILES is only used for win32")
        add_custom_target(COPY_RCEDIT32_FILES 
            COMMAND echo "COPY_RCEDIT64_FILES is only used for win32")
    endif()
endif()

set(BUILD_SHARED_LIBS ON CACHE BOOL "Build dynamic joyengine core" FORCE)

# Scan /3rd/pkg/* to build the pkg and then static link them into joyecs.

set(BAOZI_BUILD_BY_MGR OFF CACHE BOOL "Donot apply something for baozi, we will compile it ourself." FORCE)
set(BAOZI_WOOLIB_NAME "asda" CACHE STRING "Static link, don't need link woolang library." FORCE)
set(BAOZI_BUILD_SHARED OFF CACHE BOOL "Build static link library." FORCE)

macro(JE4_GET_SUB_DIR_LIST result curdir)
  file(GLOB children RELATIVE ${curdir} ${curdir}/*)
  set(dirlist "")
  foreach(child ${children})
    if(IS_DIRECTORY ${curdir}/${child})
        set(dirlist ${dirlist} ${child})
    endif()
  endforeach()
  set (${result} ${dirlist})
endmacro()

JE4_GET_SUB_DIR_LIST(JE4_STATIC_LINKING_PKG_DIR_LIST ${CMAKE_SOURCE_DIR}/3rd/pkg)
set(JE4_STATIC_LINKED_PKG_LIST "")

foreach(pkg_name ${JE4_STATIC_LINKING_PKG_DIR_LIST})
    set(pkg_fullpath "${CMAKE_SOURCE_DIR}/3rd/pkg/${pkg_name}")

    if (EXISTS "${pkg_fullpath}/CMakeLists.txt")
        message(STATUS "ready to build & link: ${pkg_name}")
        add_subdirectory("${pkg_fullpath}")

        if (TARGET ${pkg_name})
            list(APPEND JE4_STATIC_LINKED_PKG_LIST ${pkg_name})
        else()
            message(FATAL_ERROR "Target '${pkg_name}' not exist.")
        endif()
    endif()
endforeach()

########################################################################

add_subdirectory("src")

include_directories("include")
include_directories("3rd/woolang/include")

file(GLOB_RECURSE jedriver_src driver/*.cpp)
add_executable(jedriver ${jedriver_src} "include/jeecs.hpp")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
	set_target_properties(jedriver PROPERTIES RELEASE_POSTFIX "")
	set_target_properties(jedriver PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
	set_target_properties(jedriver PROPERTIES RELEASE_POSTFIX "32")
	set_target_properties(jedriver PROPERTIES DEBUG_POSTFIX "32_debug")
else()
	message(FATAL_ERROR "Unknown platform")
endif()

if (JE4_COPYING_BUILTIN_FILES)
    add_dependencies(COPY_ENGINE_FILES COPY_BUILTIN_FILES)
    add_dependencies(COPY_WOOLANG_FILES COPY_ENGINE_FILES)
    add_dependencies(COPY_RCEDIT64_FILES COPY_WOOLANG_FILES)
    add_dependencies(COPY_RCEDIT32_FILES COPY_RCEDIT64_FILES)
    add_dependencies(INSTALL_PKG COPY_RCEDIT32_FILES)
endif()
add_dependencies(jedriver INSTALL_PKG)
add_dependencies(jedriver EXECUTE_BUILD_SCRIPT)
target_link_libraries(jedriver joyecs)
