@echo off
rem Double-click on Windows: builds CleanLink in WSL and runs it.
rem Runs setup.bat first, which installs WSL, clang++ and libcurl if they are missing.

cd /d "%~dp0"

call "%~dp0setup.bat" quiet
if errorlevel 1 (
    pause
    exit /b 1
)

echo Building CleanLink...
wsl --cd "%~dp0." bash -c "clang++ -std=c++17 -Ibackend/include -Ibackend/api backend/CLI/main.cpp backend/src/*.cpp backend/api/*.cpp -lcurl -o cleanlink"
if errorlevel 1 (
    echo Build failed.
    echo If WSL or curl/curl.h is missing, run:  wsl sudo apt install -y clang libcurl4-openssl-dev
    pause
    exit /b 1
)

wsl --cd "%~dp0." ./cleanlink
pause
