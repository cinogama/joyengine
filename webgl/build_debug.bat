set emcmake_path=emcmake

if not exist build (
    mkdir build
)
cd build
call "%emcmake_path%" cmake .. -DCMAKE_BUILD_TYPE=DEBUG

@REM Check if cmake failed
if %errorlevel% neq 0 (
    cd ..
    pause
    exit /b %errorlevel%
)

call cmake --build ./ --target joyengineecs4w --verbose -- -j 32
@REM Check if build failed
if %errorlevel% neq 0 (
    cd ..
    pause
    exit /b %errorlevel%
)

cd ..
pause