set emcmake_path=emcmake

call "%emcmake_path%" cmake . -DCMAKE_BUILD_TYPE=DEBUG
@REM Check if cmake failed
if %errorlevel% neq 0 exit /b %errorlevel%

call cmake --build ./ --target joyengineecs4w --verbose -- -j 16
@REM Check if build failed
if %errorlevel% neq 0 exit /b %errorlevel%
