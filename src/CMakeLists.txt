# librscene
#
cmake_minimum_required (VERSION 3.7)

include_directories("../3rd/stb")
include_directories("../3rd/imgui")
include_directories("../3rd/imgui/backends")
include_directories("../3rd/imgui/misc/cpp")
include_directories("../3rd/box2c/include")
include_directories("../3rd/assimp/include")
include_directories("../3rd/vulkan/include")
include_directories("../3rd/glslang/glslang/Include")
include_directories("../3rd/glslang/glslang/Public")
include_directories("../3rd/spirv-cross")
include_directories("../3rd/imgui-node-editor")
include_directories("../3rd/ImGuiColorTextEdit")
include_directories("../3rd/openal-soft/include")

file(GLOB_RECURSE joyecs_srcs_cpp *.cpp)
file(GLOB_RECURSE joyecs_srcs_hpp *.hpp)

set(imgui_code_editor_srcs
    "../3rd/ImGuiColorTextEdit/TextEditor.cpp"
    "../3rd/ImGuiColorTextEdit/TextEditor.h")

set(imgui_node_editor_srcs 
    "../3rd/imgui-node-editor/crude_json.cpp"
    "../3rd/imgui-node-editor/crude_json.h"
    "../3rd/imgui-node-editor/imgui_bezier_math.h"
    "../3rd/imgui-node-editor/imgui_bezier_math.inl"
    "../3rd/imgui-node-editor/imgui_canvas.cpp"
    "../3rd/imgui-node-editor/imgui_canvas.h"
    "../3rd/imgui-node-editor/imgui_extra_math.h"
    "../3rd/imgui-node-editor/imgui_extra_math.inl"
    "../3rd/imgui-node-editor/imgui_node_editor.cpp"
    "../3rd/imgui-node-editor/imgui_node_editor.h"
    "../3rd/imgui-node-editor/imgui_node_editor_api.cpp"
    "../3rd/imgui-node-editor/imgui_node_editor_internal.h"
    "../3rd/imgui-node-editor/imgui_node_editor_internal.inl")

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

add_library(joyengine_core_configs INTERFACE)

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
if (JE4_ENABLE_OPENGL330_GAPI OR JE4_ENABLE_OPENGLES300_GAPI OR JE4_ENABLE_WEBGL20_GAPI)
    if (JE4_ENABLE_OPENGL330_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_GL330_GAPI)

        include_directories("../3rd/glew/include")
        if (WIN32)
            list(APPEND jeecs_graphic_libs OpenGL32)
        else()
            target_compile_options(glew_s PRIVATE -fPIC)
        endif()
        
        list(APPEND jeecs_graphic_libs glew_s)
    elseif (JE4_ENABLE_OPENGLES300_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_GLES300_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DIMGUI_IMPL_OPENGL_ES3)

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
    else()
        target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_WEBGL20_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DIMGUI_IMPL_OPENGL_ES3)
    endif()

    list(APPEND imgui_srcs
        "../3rd/imgui/backends/imgui_impl_opengl3.cpp"
        "../3rd/imgui/backends/imgui_impl_opengl3.h"
        "../3rd/imgui/backends/imgui_impl_opengl3_loader.h")
endif()

# DX11
if (JE4_ENABLE_DX11_GAPI AND WIN32)
    target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_DX11_GAPI)

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
if (JE4_ENABLE_VULKAN130_GAPI)
    target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_VK130_GAPI)

    list(APPEND imgui_srcs
        "../3rd/imgui/backends/imgui_impl_vulkan.cpp"
        "../3rd/imgui/backends/imgui_impl_vulkan.h")
endif()

# Metal
if (JE4_ENABLE_METAL_GAPI AND APPLE)
    target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_METAL_GAPI)
endif()

# OpenGL3.3 / OpenGLES3.0 / Vulkan needed:
if (JE4_ENABLE_OPENGL330_GAPI OR 
    JE4_ENABLE_OPENGLES300_GAPI OR 
    JE4_ENABLE_VULKAN130_GAPI OR
    JE4_ENABLE_WEBGL20_GAPI OR
    JE4_ENABLE_DX11_GAPI)

    if (ANDROID)
        list(APPEND imgui_srcs
            "../3rd/imgui/backends/imgui_impl_android.cpp"
            "../3rd/imgui/backends/imgui_impl_android.h")
    else()
        # Use built-in glfw3 for webgl target.
        if (NOT JE4_ENABLE_WEBGL20_GAPI)
            include_directories("../3rd/glfw/include")
    
            if (NOT WIN32)
                target_compile_options(glfw PRIVATE -fPIC)
            endif()

            list(APPEND jeecs_graphic_libs glfw)
        endif()
        
        list(APPEND imgui_srcs
            "../3rd/imgui/backends/imgui_impl_glfw.cpp"
            "../3rd/imgui/backends/imgui_impl_glfw.h")
    endif()
endif()

set (jeecs_core_sources 
    ${joyecs_srcs_cpp}
    ${joyecs_srcs_hpp} 
    ${imgui_srcs} 
    ${imgui_node_editor_srcs}
    ${imgui_code_editor_srcs}
    "../include/jeecs.hpp")

# Build engine shared/static library
if (BUILD_SHARED_LIBS)
    add_library(joyecs SHARED ${jeecs_core_sources})
    if(UNIX)
        target_compile_options(joyecs PRIVATE -Wall -rdynamic)
    endif()
else()
    add_library(joyecs STATIC ${jeecs_core_sources})
endif()

set_target_properties(joyecs PROPERTIES OUTPUT_NAME libjoyecs)
set_target_properties(joyecs PROPERTIES PREFIX "")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "")
    set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "_debug")
elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
    # NOTE: In 32bit platform, we need use ImU64 to replace ImTextureID for vulkan backend.
    target_compile_definitions(joyecs PRIVATE -DImTextureID=ImU64)

    set_target_properties(joyecs PROPERTIES RELEASE_POSTFIX "32")
    set_target_properties(joyecs PROPERTIES DEBUG_POSTFIX "32_debug")
else()
    message(FATAL_ERROR "Unknown platform")
endif()

if (NOT WIN32)
    target_compile_options (box2d PRIVATE -fPIC)
endif()

target_link_libraries(joyecs 
    PRIVATE joyengine_configs
    PRIVATE joyengine_core_configs
    PRIVATE ${jeecs_system_libs}
    PRIVATE ${jeecs_graphic_libs}
    PRIVATE ${JE4_STATIC_LINKED_PKG_AND_MODULE_LIST}
    PRIVATE woolang
    PRIVATE box2d 
    PRIVATE assimp 
    PRIVATE glslang
    PRIVATE SPIRV 
    # PRIVATE spirv-cross-c
    PRIVATE glslang-default-resource-limits)

if (JE4_WASM_TARGET)
    # Use emscripten openal for wasm target.
    # https://emscripten.org/docs/porting/Audio.html 
    target_link_libraries(joyecs PRIVATE openal)
else()
    target_link_libraries(joyecs PRIVATE OpenAL)
endif()

