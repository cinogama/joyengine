cmake_minimum_required(VERSION 3.22.1)

project("joyengineecs4w")

# Setting for JoyEngineECS 4 Webgl.
set(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD OFF CACHE BOOL "Disable install baozi on WebGL" FORCE)
set(JE4_COPYING_BUILTIN_FILES           OFF CACHE BOOL "Disable copy builtin files on WebGL" FORCE)
set(JE4_STATIC_LINK_MODULE_AND_PKGS     ON  CACHE BOOL "Enable static link packages on WebGL." FORCE)
set(JE4_BUILD_SHARED_CORE               OFF CACHE BOOL "Disable static engine core and WebGL on Android." FORCE)

set(JE4_ENABLE_DX11_GAPI                OFF CACHE BOOL "Disable DX11 on WebGL" FORCE)
set(JE4_ENABLE_OPENGL330_GAPI           OFF CACHE BOOL "Disable OpenGL 3.3 on WebGL" FORCE)
set(JE4_ENABLE_OPENGLES300_GAPI         OFF CACHE BOOL "Enable OpenGLES 3.0 on WebGL" FORCE)
set(JE4_ENABLE_WEBGL20_GAPI             ON CACHE BOOL "Disable WebGL 2.0 on WebGL" FORCE)
set(JE4_ENABLE_VULKAN130_GAPI           OFF CACHE BOOL "Disable Vulkan1.3 on WebGL" FORCE)
set(JE4_ENABLE_METAL_GAPI               OFF CACHE BOOL "Disable Metal on WebGL" FORCE)

set(JE4_WASM_TARGET                     ON CACHE BOOL "Enable WASM target" FORCE)

set(WO_SUPPORT_ASMJIT                   OFF CACHE BOOL "Disable JIT on WebGL" FORCE)
set(WO_DISABLE_FUNCTION_FOR_WASM        ON CACHE BOOL "Disable dynamic lib loading" FORCE)
set(BOX2D_AVX2                          OFF CACHE BOOL "Disable avx2 for box2d." FORCE)

add_compile_options(-pthread)

# Searches for a package provided by the game activity dependency
set(JE4WEBGL_PATH "./../")
add_subdirectory(${JE4WEBGL_PATH} joyengineecs.out)

include_directories("${JE4WEBGL_PATH}/include")
include_directories("${JE4WEBGL_PATH}/3rd/woolang/include")

add_executable(joyengineecs4w main.cpp)
target_link_libraries(joyengineecs4w
        # JoyEngine core
        PUBLIC joyecs
        PRIVATE woolang)

# Use wasm64

set_target_properties(joyengineecs4w PROPERTIES
        SUFFIX ".html"
        OUTPUT_NAME "index"
        LINK_FLAGS "-s WASM=1 -s USE_PTHREADS=1 -s SHARED_MEMORY=1 -s PTHREAD_POOL_SIZE=16 -s USE_WEBGL2=1 -s USE_GLFW=3 -s ALLOW_MEMORY_GROWTH -s STACK_SIZE=4194304")
