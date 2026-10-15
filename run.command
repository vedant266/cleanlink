#!/bin/bash
# Double-click on macOS: opens Terminal, builds CleanLink and runs it.

cd "$(dirname "$0")" || exit 1

echo "Building CleanLink..."
if ! clang++ -std=c++17 \
    -Ibackend/include -Ibackend/api \
    backend/CLI/main.cpp backend/src/*.cpp backend/api/*.cpp \
    -lcurl -o cleanlink
then
    echo "Build failed."
    read -n 1 -s -r -p "Press any key to close..."
    exit 1
fi

./cleanlink
