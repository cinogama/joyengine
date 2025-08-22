# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html

cmake_minimum_required(VERSION 3.22.1)

project("joyengineecs4a")

# Setting for JoyEngineECS 4 Android.
set(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD OFF CACHE BOOL "Disable install baozi on Android" FORCE)
set(JE4_COPYING_BUILTIN_FILES           OFF CACHE BOOL "Disable copy builtin files on Android" FORCE)
set(JE4_STATIC_LINK_MODULE_AND_PKGS     ON  CACHE BOOL "Enable static link packages on Android." FORCE)
set(JE4_BUILD_SHARED_CORE               OFF CACHE BOOL "Disable static engine core and woolang on Android." FORCE)

set(JE4_ENABLE_SHADER_WRAP_GENERATOR    OFF  CACHE BOOL "Disable shader generator and library on Android" FORCE)

set(JE4_ENABLE_DX11_GAPI                OFF CACHE BOOL "Disable DX11 on Android" FORCE)
set(JE4_ENABLE_OPENGL330_GAPI           OFF CACHE BOOL "Disable OpenGL 3.3 on Android" FORCE)
set(JE4_ENABLE_OPENGLES300_GAPI         ON CACHE BOOL "Enable OpenGLES 3.0 on Android" FORCE)
set(JE4_ENABLE_WEBGL20_GAPI             OFF CACHE BOOL "Disable WebGL 2.0 on Android" FORCE)
set(JE4_ENABLE_VULKAN130_GAPI           OFF CACHE BOOL "Disable Vulkan1.3 on Android" FORCE)
set(JE4_ENABLE_METAL_GAPI               OFF CACHE BOOL "Disable Metal on Android" FORCE)

set(WO_DISABLE_COMPILER                 ON CACHE BOOL "Disable compiler on Android to reduce size" FORCE)
set(WO_SUPPORT_ASMJIT                   OFF CACHE BOOL "Disable JIT on Android" FORCE)
set(WO_DISABLE_FUNCTION_FOR_WASM        ON CACHE BOOL "Disable dynamic lib loading" FORCE)

# Searches for a package provided by the game activity dependency
find_package(game-activity REQUIRED CONFIG)

# Add sub module!
set(JE4ANDROID_PATH "./../../../../..")
add_subdirectory(${JE4ANDROID_PATH} joyengineecs.out)

include_directories("${JE4ANDROID_PATH}/include")
include_directories("${JE4ANDROID_PATH}/3rd/woolang/include")

target_link_libraries(joyecs PUBLIC game-activity::game-activity)
target_link_libraries(joyecs
        # EGL and other dependent libraries required for drawing
        # and interacting with Android system
        PUBLIC EGL
        PUBLIC GLESv3
        PUBLIC jnigraphics
        PUBLIC android
        PUBLIC log)

# Creates your game shared library. The name must be the same as the
# one used for loading in your Kotlin/Java or AndroidManifest.txt files.
add_library(joyengineecs4a SHARED main.cpp)

# Configure libraries CMake uses to link your target library.
target_link_libraries(joyengineecs4a
        # JoyEngine core
        PUBLIC joyecs
        PRIVATE woolang)