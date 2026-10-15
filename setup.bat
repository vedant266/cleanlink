@echo off
rem Checks that WSL, clang++ and libcurl are ready, and installs whatever is missing.
rem run.bat and web.bat call this first; you can also double-click it on its own.

wsl -e true >nul 2>nul
if errorlevel 1 (
    echo WSL is not set up. Installing WSL with Ubuntu...
    wsl --install -d Ubuntu
    echo.
    echo Restart your PC if asked, then open Ubuntu once from the Start menu
    echo to create a username and password. After that, run this again.
    if "%~1"=="" pause
    exit /b 1
)

wsl -e sh -c "command -v clang++ >/dev/null && echo '#include <curl/curl.h>' | clang++ -E -x c++ - >/dev/null 2>&1"
if errorlevel 1 (
    echo Installing clang and libcurl in WSL - it will ask for your Ubuntu password...
    wsl -e sh -c "sudo apt-get update && sudo apt-get install -y clang libcurl4-openssl-dev"
    if errorlevel 1 (
        echo Install failed. Try running this in a terminal:
        echo   wsl sudo apt-get install -y clang libcurl4-openssl-dev
        if "%~1"=="" pause
        exit /b 1
    )
)

echo Setup OK: WSL, clang++ and libcurl are ready.
if "%~1"=="" pause
exit /b 0
