set emcmake_path=emcmake

if not exist build (
    mkdir build
)

if not exist output (
    mkdir output
    mkdir output\icon

    @REM Copy ../builtin/icon/* to ./output/icon/
    xcopy /s /e /y ..\builtin\icon output\icon

    @REM Copy ./template/index.html to ./output/index.html
    copy template\index.html output\index.html
    copy template\fullscreen.svg output\icon\fullscreen.svg
)
cd build
call "%emcmake_path%" cmake .. -DCMAKE_BUILD_TYPE=DEBUG

@REM Check if cmake failed
if %errorlevel% neq 0 (
    cd ..
    pause
    exit /b %errorlevel%
)

call cmake --build ./ --config=DEBUG --target joyengineecs4w --parallel
@REM Check if build failed
if %errorlevel% neq 0 (
    cd ..
    pause
    exit /b %errorlevel%
)

cd ..
pause