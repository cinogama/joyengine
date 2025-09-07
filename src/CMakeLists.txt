# librscene
#
cmake_minimum_required (VERSION 3.10)

set(GIT_COMMIT_HASH "Unavailable")
find_package(Git)
if(GIT_FOUND)
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse HEAD
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/..
        OUTPUT_VARIABLE GIT_COMMIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
file(WRITE ${CMAKE_CURRENT_SOURCE_DIR}/jeecs_commit_sha.hpp
    "\"${GIT_COMMIT_HASH}\"\n")

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

target_include_directories(joyengine_core_configs INTERFACE 
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/imgui"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/imgui/backends"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/imgui/misc/cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/box2d/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/assimp/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/glslang/glslang/Include"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/glslang/glslang/Public"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/spirv-cross"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/imgui-node-editor"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/ImGuiColorTextEdit"
    "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/openal-soft/include")

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

        target_include_directories(joyengine_core_configs INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/glew/include")

        if (MSVC)
            list(APPEND jeecs_graphic_libs OpenGL32)
        elseif (JE4_BUILD_SHARED_CORE)
            target_compile_options(glew_s PRIVATE -fPIC)
        endif()
        
        list(APPEND jeecs_graphic_libs glew_s)
    elseif (JE4_ENABLE_OPENGLES300_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DJE_ENABLE_GLES300_GAPI)
        target_compile_definitions(joyengine_core_configs INTERFACE -DIMGUI_IMPL_OPENGL_ES3)

        if (WIN32)
            target_include_directories(joyengine_core_configs INTERFACE 
                "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/opengles_env_w64/include")
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

# Metal
if (JE4_ENABLE_METAL_GAPI AND APPLE)
    find_library(METAL_FRAMEWORK Metal
        PATHS "../3rd/metal-cpp"
        REQUIRED)
    find_library(METALKIT_FRAMEWORK MetalKit
        PATHS "../3rd/metal-cpp"
        REQUIRED)
    find_library(APPKIT_FRAMEWORK AppKit
        PATHS "../3rd/metal-cpp"
        REQUIRED)
    find_library(FOUNDATION_FRAMEWORK Foundation
        PATHS "../3rd/metal-cpp"
        REQUIRED)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore
        PATHS "../3rd/metal-cpp"
        REQUIRED)

    target_link_libraries(joyengine_core_configs INTERFACE
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${APPKIT_FRAMEWORK}
        ${FOUNDATION_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK})

    target_include_directories(joyengine_core_configs INTERFACE 
        "../3rd/metal-cpp")
endif()

# Vulkan 1.3
if (JE4_ENABLE_VULKAN130_GAPI)
    target_compile_definitions(joyengine_core_configs INTERFACE
        -DJE_ENABLE_VK130_GAPI)

    list(APPEND imgui_srcs
        "../3rd/imgui/backends/imgui_impl_vulkan.cpp"
        "../3rd/imgui/backends/imgui_impl_vulkan.h")
 
    if (APPLE)
        # Use MoltenVK for macOS/iOS.
        target_include_directories(joyengine_core_configs INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/moltenvk/MoltenVK/include")

        # Find static lib in ../3rd/moltenvk/MoltenVK/static    
        set(JE4_MOLTENVK_STATIC_LIBS 
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/moltenvk/MoltenVK/static")

        find_library(MOLTENVK_FRAMEWORK
            NAMES MoltenVK
            PATHS "${JE4_MOLTENVK_STATIC_LIBS}"
            NO_DEFAULT_PATH)
        
        if(MOLTENVK_FRAMEWORK)
            target_link_libraries(joyengine_core_configs INTERFACE 
                ${MOLTENVK_FRAMEWORK}
                "-framework Metal"
                "-framework QuartzCore"
                "-framework IOSurface"
                "-framework IOKit"
                "-framework Foundation")
        else()
            message(FATAL_ERROR "MoltenVK framework not found")
        endif()
    else()
         target_include_directories(joyengine_core_configs INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/vulkan/include")
        target_compile_definitions(joyengine_core_configs INTERFACE
            -DIMGUI_IMPL_VULKAN_NO_PROTOTYPES)
    endif()
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
        target_include_directories(joyengine_core_configs INTERFACE 
            "${CMAKE_CURRENT_SOURCE_DIR}/../3rd/glfw/include")

        if (NOT JE4_ENABLE_WEBGL20_GAPI)
            # Use built-in glfw3 for webgl target, so we don't need to build glfw3.
            if (JE4_BUILD_SHARED_CORE AND NOT MSVC)
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

if (MSVC)
    list(APPEND jeecs_core_sources resources.rc)
endif()

# Build engine shared/static library
if (BUILD_SHARED_LIBS)
    add_library(joyecs SHARED ${jeecs_core_sources})
    if(UNIX)
        target_compile_options(joyecs PRIVATE -Wall)
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

if (JE4_BUILD_SHARED_CORE AND NOT MSVC)
    target_compile_options (box2d PRIVATE -fPIC)
endif()

if (JE4_ENABLE_SHADER_WRAP_GENERATOR)
    # Build glslang and spirv-cross-c for shader wrap generator.
    target_compile_definitions(joyecs PRIVATE -DJE4_ENABLE_SHADER_WRAP_GENERATOR)

    target_compile_features(spirv-cross-c PRIVATE cxx_std_17)
    target_compile_features(spirv-cross-core PRIVATE cxx_std_17)
    target_compile_features(spirv-cross-msl PRIVATE cxx_std_17)
    if (JE4_BUILD_SHARED_CORE AND NOT MSVC)
        target_compile_options (spirv-cross-c PRIVATE -fPIC)
        target_compile_options (spirv-cross-cpp PRIVATE -fPIC)
        target_compile_options (spirv-cross-hlsl PRIVATE -fPIC)
        target_compile_options (spirv-cross-glsl PRIVATE -fPIC)
        target_compile_options (spirv-cross-msl PRIVATE -fPIC)
        target_compile_options (spirv-cross-core PRIVATE -fPIC)
        target_compile_options (spirv-cross-reflect PRIVATE -fPIC)
    endif()

    list(APPEND jeecs_graphic_libs 
        glslang
        SPIRV
        spirv-cross-c
        glslang-default-resource-limits)
endif()

target_link_libraries(joyecs PRIVATE 
    PUBLIC joyengine_configs
    PUBLIC joyengine_core_configs
    ${jeecs_system_libs}
    ${jeecs_graphic_libs}
    ${JE4_STATIC_LINKED_PKG_AND_MODULE_LIST}
    woolang
    box2d 
    assimp)

if (JE4_SPECIFY_OPENAL_LIB_NAME STREQUAL "")
    target_link_libraries(joyecs PRIVATE OpenAL)
else()
    # Use specify openal for some target.
    # E.G. WASM: https://emscripten.org/docs/porting/Audio.html 
    target_link_libraries(joyecs PRIVATE ${JE4_SPECIFY_OPENAL_LIB_NAME})
endif()

