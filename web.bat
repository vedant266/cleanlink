@echo off
rem Double-click on Windows: builds the CleanLink web server in WSL and opens it in the browser.
rem Runs setup.bat first, which installs WSL, clang++ and libcurl if they are missing.

cd /d "%~dp0"

call "%~dp0setup.bat" quiet
if errorlevel 1 (
    pause
    exit /b 1
)

echo Building CleanLink web...
wsl --cd "%~dp0." bash -c "clang++ -std=c++17 -Ibackend/include -Ibackend/api backend/web/server.cpp backend/src/*.cpp backend/api/*.cpp -lcurl -o cleanlink-web"
if errorlevel 1 (
    echo Build failed.
    echo If WSL or curl/curl.h is missing, run:  wsl sudo apt install -y clang libcurl4-openssl-dev
    pause
    exit /b 1
)

start "" /min powershell -NoProfile -Command "Start-Sleep 1; Start-Process 'http://localhost:8080'"
wsl --cd "%~dp0." ./cleanlink-web
