#!/bin/bash
# Double-click on macOS: builds the CleanLink web server and opens it in the browser.

cd "$(dirname "$0")" || exit 1

echo "Building CleanLink web..."
if ! clang++ -std=c++17 \
    -Ibackend/include -Ibackend/api \
    backend/web/server.cpp backend/src/*.cpp backend/api/*.cpp \
    -lcurl -o cleanlink-web
then
    echo "Build failed."
    read -n 1 -s -r -p "Press any key to close..."
    exit 1
fi

(sleep 1 && open http://localhost:8080) &
./cleanlink-web
