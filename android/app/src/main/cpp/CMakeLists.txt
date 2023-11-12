# For more information about using CMake with Android Studio, read the
# documentation: https://d.android.com/studio/projects/add-native-code.html.
# For more examples on how to use CMake, see https://github.com/android/ndk-samples.

# Sets the minimum CMake version required for this project.
cmake_minimum_required(VERSION 3.22.1)

# Declares the project name. The project name can be accessed via ${ PROJECT_NAME},
# Since this is the top level CMakeLists.txt, the project name is also accessible
# with ${CMAKE_PROJECT_NAME} (both CMake variables are in-sync within the top level
# build script scope).
project("joyengineecs4a")

# Setting for JoyEngineECS 4 Android.
set(JE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD OFF CACHE BOOL "Disable install baozi on Android" FORCE)
set(JE4_COPYING_BUILTIN_FILES OFF CACHE BOOL "Disable copy builtin files on Android" FORCE)
set(JE4_EXECUTE_BUILD_SCRIPT OFF CACHE BOOL "Disable build script on Android" FORCE)

set(JE4_ENABLE_DX11_GAPI OFF CACHE BOOL "Disable DX11 on Android" FORCE)
set(JE4_ENABLE_OPENGL330_GAPI OFF CACHE BOOL "Disable OpenGL 3.3 on Android" FORCE)
set(JE4_ENABLE_OPENGLES300_GAPI ON CACHE BOOL "Enable OpenGLES 3.0 on Android" FORCE)
set(JE4_ENABLE_VULKAN110_GAPI OFF CACHE BOOL "Disable Vulkan1.1 on Android" FORCE)
set(JE4_ENABLE_METAL_GAPI OFF CACHE BOOL "Disable Metal on Android" FORCE)

# Add sub module!
set(JE4ANDROID_PATH "./../../../../..")
add_subdirectory(${JE4ANDROID_PATH} joyengineecs.out)

include_directories("${JE4ANDROID_PATH}/include")
include_directories("${JE4ANDROID_PATH}/3rd/woolang/include")


add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        android_jedriver.cpp)

add_dependencies(${CMAKE_PROJECT_NAME} jedriver)
target_link_libraries(${CMAKE_PROJECT_NAME}
        joyecs
        # List libraries link to the target library
        android
        log)