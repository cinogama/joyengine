start start_server_only.bat

@REM Wait 2s by ping
ping 127.0.0.1 -n 2 > nul

@REM open http://127.0.0.1:8000
start http://127.0.0.1:8000