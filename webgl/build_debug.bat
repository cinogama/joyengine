set emcmake_path=emcmake

if not exist build (
    mkdir build
    
    @REM Copy ../builtin/icon/* to ./build/icon/
    mkdir build\icon
    xcopy /s /e /y ..\builtin\icon build\icon

    @REM Copy ./template/index.html to ./build/index.html
    copy template\index.html build\index.html
    copy template\fullscreen.svg build\icon\fullscreen.svg
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