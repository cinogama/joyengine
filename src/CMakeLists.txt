# librscene
#
cmake_minimum_required (VERSION 3.16)

include_directories("../include")
include_directories("../3rd/woolang/include")
include_directories("../3rd/stb")
include_directories("../3rd/imgui")
include_directories("../3rd/imgui/backends")
include_directories("../3rd/imgui/misc/cpp")
include_directories("../3rd/box2d/include")
include_directories("../3rd/openal-soft/include")
include_directories("../3rd/assimp/include")

file(GLOB_RECURSE joyecs_srcs_cpp *.cpp)
file(GLOB_RECURSE joyecs_srcs_hpp *.hpp)

set(jeecs_system_libs "")
set(jeecs_graphic_libs "")

if (WIN32)
    list(APPEND jeecs_system_libs winmm)
else()
    if(NOT ANDROID)
        list(APPEND jeecs_system_libs pthread)
    endif()
    list(APPEND jeecs_system_libs dl)
endif()

set(imgui_srcs    
    "../3rd/imgui/imconfig.h"
    "../3rd/imgui/imgui.cpp"
    "../3rd/imgui/imgui_draw.cpp"
    "../3rd/imgui/imgui.h"
    "../3rd/imgui/imgui_internal.h"
    "../3rd/imgui/imgui_tables.cpp"
    "../3rd/imgui/imgui_widgets.cpp"
    "../3rd/imgui/misc/cpp/imgui_stdlib.cpp"
    "../3rd/imgui/misc/cpp/imgui_stdlib.h")

# JoyEngine Graphic API support list.
# OpenGL 3.3 & OpenGLES 3.2
if (JE4_ENABLE_OPENGL330_GAPI OR JE4_ENABLE_OPENGLES300_GAPI)
    if (JE4_ENABLE_OPENGL330_GAPI)
        add_definitions(-DJE_ENABLE_GL330_GAPI)

        include_directories("../3rd/glew/include")
        if (WIN32)
            list(APPEND jeecs_graphic_libs OpenGL32)
        else()
            target_compile_options(glew_s PRIVATE -fPIC)
        endif()
        
        list(APPEND jeecs_graphic_libs glew_s)
        
    else()
        add_definitions(-DJE_ENABLE_GLES300_GAPI)
        add_definitions(-DIMGUI_IMPL_OPENGL_ES3)

        if (WIN32)
            include_directories("../3rd/opengles_env_w64/include")
            list(APPEND jeecs_graphic_libs "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/opengles_env_w64/libEGL.lib")
            list(APPEND jeecs_graphic_libs "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/opengles_env_w64/libGLESv2.lib")
        else()
            if (ANDROID)
                list(APPEND jeecs_graphic_libs GLESv3)
            else()
                list(APPEND jeecs_graphic_libs GLESv2)
            endif()
            list(APPEND jeecs_graphic_libs EGL)
        endif()

    endif()

    list(APPEND imgui_srcs
        "../3rd/imgui/backends/imgui_impl_opengl3.cpp"
        "../3rd/imgui/backends/imgui_impl_opengl3.h"
        "../3rd/imgui/backends/imgui_impl_opengl3_loader.h")
endif()

# DX11
if (JE4_ENABLE_DX11_GAPI AND WIN32)
    add_definitions(-DJE_ENABLE_DX11_GAPI)

    list(APPEND jeecs_graphic_libs d3d11)
    list(APPEND jeecs_graphic_libs dxgi)
    list(APPEND jeecs_graphic_libs dxguid)
    list(APPEND jeecs_graphic_libs d3dcompiler)

    list(APPEND imgui_srcs
        "../3rd/imgui/backends/imgui_impl_win32.cpp"
        "../3rd/imgui/backends/imgui_impl_win32.h"
        "../3rd/imgui/backends/imgui_impl_dx11.cpp"
        "../3rd/imgui/backends/imgui_impl_dx11.h")
endif()

# Vulkan 1.1
if (JE4_ENABLE_VULKAN110_GAPI)
    add_definitions(-DJE_ENABLE_VK110_GAPI)
endif()

# Metal
if (JE4_ENABLE_METAL_GAPI AND APPLE)
    add_definitions(-DJE_ENABLE_METAL_GAPI)
endif()

# OpenGL3.3 / OpenGLES3.2 / Vulkan needed:
if (JE4_ENABLE_OPENGL330_GAPI OR 
    JE4_ENABLE_OPENGLES300_GAPI OR 
    JE4_ENABLE_VULKAN110_GAPI)

    if (ANDROID)
        list(APPEND imgui_srcs
            "../3rd/imgui/backends/imgui_impl_android.cpp"
            "../3rd/imgui/backends/imgui_impl_android.h")
    else()
        include_directories("../3rd/glfw/include")
    
        if (NOT WIN32)
            target_compile_options(glfw PRIVATE -fPIC)
        endif()

        list(APPEND jeecs_graphic_libs glfw)
        list(APPEND imgui_srcs
            "../3rd/imgui/backends/imgui_impl_glfw.cpp"
            "../3rd/imgui/backends/imgui_impl_glfw.h")
    endif()
endif()

# Build engine shared/static library
if (${BUILD_SHARED_LIBS})
    add_definitions(-DWO_SHARED_LIB)
    add_library(joyecs SHARED ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_srcs} "../include/jeecs.hpp")
    if(UNIX)
        add_compile_options(-Wall -rdynamic)
endif()
else()
    add_definitions(-DWO_STATIC_LIB)
    add_library(joyecs STATIC ${joyecs_srcs_cpp} ${joyecs_srcs_hpp} ${imgui_srcs} "../include/jeecs.hpp")
endif()

set_target_properties(joyecs PROPERTIES OUTPUT_NAME libjoyecs)
set_target_properties(joyecs PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "32_debug")
else()
    message(FATAL_ERROR "Unknown platform")
endif()

if (NOT WIN32)
    target_compile_options (box2d PRIVATE -fPIC)
endif()
target_link_libraries(joyecs ${jeecs_system_libs})
target_link_libraries(joyecs ${jeecs_graphic_libs})
target_link_libraries(joyecs woolang box2d OpenAL assimp)
