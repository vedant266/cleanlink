# Recipe Render uses to build and run the CleanLink website.

# 1. Start from Ubuntu and install a C++ compiler and libcurl (used for reverse geocoding)
FROM ubuntu:24.04
RUN apt-get update && apt-get install -y g++ libcurl4-openssl-dev ca-certificates \
    && rm -rf /var/lib/apt/lists/*

# 2. Copy the project in
WORKDIR /app
COPY backend backend
COPY frontend frontend

# 3. Compile the web server (same command as web.command, with g++ instead of clang++)
RUN g++ -std=c++17 -Ibackend/include -Ibackend/api \
    backend/web/server.cpp backend/src/*.cpp backend/api/*.cpp \
    -lcurl -o cleanlink-web

# 4. Start it. Render sets the PORT variable, server.cpp reads it.
CMD ["./cleanlink-web"]
