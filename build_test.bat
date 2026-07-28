if not exist cmakebuild ( mkdir cmakebuild )
cd cmakebuild
cmake .. -A x64 -DJE4_INSTALL_PKG_BY_BAOZI_WHEN_BUILD=ON -DJE4_COPYING_BUILTIN_FILES=ON -DJE4_STATIC_LINK_MODULE_AND_PKGS=OFF -DJE4_BUILD_SHARED_CORE=ON -DCMAKE_BUILD_TYPE=DEBUG
cmake --build . --config=DEBUG --target jedriver --parallel