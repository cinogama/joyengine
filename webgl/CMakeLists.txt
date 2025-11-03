cmake_minimum_required(VERSION 3.22.1)

project("joyengineecs4w")

# Setting for JoyEngineECS 4 Webgl.
set(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD OFF CACHE BOOL "Disable install baozi on WebGL" FORCE)
set(JE4_STATIC_LINK_MODULE_AND_PKGS     ON  CACHE BOOL "Enable static link packages on WebGL" FORCE)
set(JE4_BUILD_SHARED_CORE               OFF CACHE BOOL "Disable static engine core on WebGL" FORCE)

set(JE4_ENABLE_SHADER_WRAP_GENERATOR    OFF  CACHE BOOL "Disable shader generator on WebGL" FORCE)

set(JE4_ENABLE_DX11_GAPI                OFF CACHE BOOL "Disable DX11 on WebGL" FORCE)
set(JE4_ENABLE_OPENGL330_GAPI           OFF CACHE BOOL "Disable OpenGL 3.3 on WebGL" FORCE)
set(JE4_ENABLE_OPENGLES300_GAPI         OFF CACHE BOOL "Disable OpenGLES 3.0 on WebGL" FORCE)
set(JE4_ENABLE_WEBGL20_GAPI             ON CACHE BOOL "Enable WebGL 2.0 on WebGL" FORCE)
set(JE4_ENABLE_VULKAN130_GAPI           OFF CACHE BOOL "Disable Vulkan1.3 on WebGL" FORCE)
set(JE4_ENABLE_METAL_GAPI               OFF CACHE BOOL "Disable Metal on WebGL" FORCE)

set(WO_DISABLE_COMPILER                 ON CACHE BOOL "Disable compiler on WebGL to reduce size" FORCE)
set(WO_SUPPORT_ASMJIT                   OFF CACHE BOOL "Disable JIT on WebGL" FORCE)
set(WO_DISABLE_FUNCTION_FOR_WASM        ON CACHE BOOL "Disable dynamic lib loading" FORCE)

set(JE4_IGNORED_WOOLANG_PKG_LIST        "process;socket" CACHE STRING "Ignore process and socket for WASM" FORCE)
set(JE4_SPECIFY_OPENAL_LIB_NAME         "openal" CACHE STRING "Specify openal lib name" FORCE)

add_compile_options(-pthread)

# WASM64 is not supported well, so we use WASM32 as default.
if (OFF)
    add_compile_options(-m64)
    set(WO_SPECIFY_PLATFORM_TYPE "WO_PLATFORM_64" CACHE STRING "Specify platform type." FORCE)
    set(JE4WEBGL_MEMORY64 1)
else()
    add_compile_options(-m32)
    set(WO_SPECIFY_PLATFORM_TYPE "WO_PLATFORM_32" CACHE STRING "Specify platform type." FORCE)
    set(JE4WEBGL_MEMORY64 0)
endif()

# Searches for a package provided by the game activity dependency
set(JE4WEBGL_PATH "./../")
add_subdirectory(${JE4WEBGL_PATH} joyengineecs.out)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/output")
set(LIBRARY_OUTPUT_PATH "${CMAKE_CURRENT_SOURCE_DIR}/output")

add_executable(joyengineecs4w main.cpp)
target_link_libraries(joyengineecs4w
    # JoyEngine core
    PUBLIC joyecs
    PRIVATE woolang idbfs.js)

# Use wasm64
file(GLOB PRELOAD_FILES 
    RELATIVE 
    "${CMAKE_CURRENT_SOURCE_DIR}/preloads"
    "${CMAKE_CURRENT_SOURCE_DIR}/preloads/*")
set(JE4_PRELOAD_FLAGS "")

foreach(file ${PRELOAD_FILES})
    set(JE4_PRELOAD_FLAGS 
        "${JE4_PRELOAD_FLAGS} \
        --preload-file \"${CMAKE_CURRENT_SOURCE_DIR}/preloads/${file}\"@/${file}")
endforeach()

target_include_directories(
    joyengineecs4w    
    PRIVATE "${JE4WEBGL_PATH}/include"
    PRIVATE "${JE4WEBGL_PATH}/3rd/woolang/include")

string (TOUPPER "${CMAKE_BUILD_TYPE}" JE_WEBGL_CMAKE_BUILD_TYPE_UPPER_CASE)
if (JE_WEBGL_CMAKE_BUILD_TYPE_UPPER_CASE STREQUAL "DEBUG"
    OR JE_WEBGL_CMAKE_BUILD_TYPE_UPPER_CASE STREQUAL "RELWITHDEBINFO")
    set(JE4_PRELOAD_FLAGS 
        "${JE4_PRELOAD_FLAGS} \
        -s ASSERTIONS=1 \
        -s SAFE_HEAP=1 \
        -s STACK_OVERFLOW_CHECK=1 \
        -s GL_ASSERTIONS=1 \
        -s GL_DEBUG=1")
endif()

set_target_properties(joyengineecs4w PROPERTIES
        SUFFIX ".html"
        LINK_FLAGS 
        "-s WASM=1 \
        -s MEMORY64=${JE4WEBGL_MEMORY64} \
        -s USE_PTHREADS=1 \
        -s USE_WEBGL2=1 \
        -s USE_GLFW=3 \
        -s SHARED_MEMORY=1 \
        -s INITIAL_MEMORY=33554432 \
        -s ALLOW_MEMORY_GROWTH=1 \
        -s STACK_SIZE=1048576 \
        -s DEFAULT_PTHREAD_STACK_SIZE=1048576 \
        -s PTHREAD_POOL_SIZE=16 \
        -s PTHREAD_POOL_SIZE_STRICT=0 \
        -s MIN_SAFARI_VERSION=150000 \
        -s MIN_CHROME_VERSION=85 \
        -s MIN_FIREFOX_VERSION=79 \
        -s EXPORTED_RUNTIME_METHODS=[\"cwrap\",\"requestFullscreen\"] \
        ${JE4_PRELOAD_FLAGS}")
