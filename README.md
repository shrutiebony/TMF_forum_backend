# TMF_forum_BackEnd

A tool in the telecom domain that converts API requests and responses to a TMF compliant API.

The service is a C++ application built with CMake. It listens on port 1001 and uses the existing MongoDB database `TMF_FORUM_POC` on `localhost:27017`. Endpoints, request and response shapes, and collection layouts are unchanged.

## Build

Requires CMake 3.20+, a C++17 compiler, and Git (CMake downloads nlohmann/json, cpp-httplib, and mongo-c-driver).

```
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release
```

The server binary is `build/Release/tmf_forum_backend` (or `build/tmf_forum_backend` with a single-config generator).

## Run

Start MongoDB locally, then run `tmf_forum_backend`. The process listens on port 1001.
