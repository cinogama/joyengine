cmake_minimum_required(VERSION 3.22.1)

project("joyengineecs4m")

# Setting for JoyEngineECS 4 MacOS.
set(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD ON CACHE BOOL "Enable install baozi on MacOS" FORCE)
set(JE4_STATIC_LINK_MODULE_AND_PKGS ON  CACHE BOOL "Enable static link packages on MacOS." FORCE)
set(JE4_BUILD_SHARED_CORE           OFF CACHE BOOL "Disable static engine core and woolang on MacOS." FORCE)

set(JE4_ENABLE_SHADER_WRAP_GENERATOR ON  CACHE BOOL "Disable shader generator and library on MacOS" FORCE)

set(JE4_ENABLE_DX11_GAPI            OFF CACHE BOOL "Disable DX11 on MacOS" FORCE)
set(JE4_ENABLE_OPENGL330_GAPI       OFF CACHE BOOL "Disable OpenGL 3.3 on MacOS" FORCE)
set(JE4_ENABLE_OPENGLES300_GAPI     OFF CACHE BOOL "Disable OpenGLES 3.0 on MacOS" FORCE)
set(JE4_ENABLE_WEBGL20_GAPI         OFF CACHE BOOL "Disable WebGL 2.0 on MacOS" FORCE)
set(JE4_ENABLE_VULKAN130_GAPI       OFF CACHE BOOL "Disable Vulkan1.3 on MacOS" FORCE)
set(JE4_ENABLE_METAL_GAPI           ON CACHE BOOL "Enable Metal on MacOS" FORCE)

set(WO_SUPPORT_ASMJIT               OFF CACHE BOOL "Disable JIT on MacOS" FORCE)

# Searches for a package provided by the game activity dependency
set(JE4MACOS_PATH "./../")
add_subdirectory(${JE4MACOS_PATH} joyengineecs.out)

include_directories("${JE4MACOS_PATH}/include")
include_directories("${JE4MACOS_PATH}/3rd/woolang/include")

add_executable(joyengineecs4m main.cpp)
target_link_libraries(joyengineecs4m
        # JoyEngine core
        PUBLIC joyecs
        PRIVATE woolang)
