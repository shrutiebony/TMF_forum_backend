cmake_minimum_required(VERSION 3.20)
project(TMF_Forum_Backend VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    add_compile_options(/utf-8 /EHsc)
endif()

include(FetchContent)

FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
    GIT_SHALLOW TRUE
)
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG v0.18.3
    GIT_SHALLOW TRUE
)
set(HTTPLIB_REQUIRE_OPENSSL OFF CACHE BOOL "" FORCE)
set(HTTPLIB_REQUIRE_ZLIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(httplib)

set(ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(ENABLE_EXAMPLES OFF CACHE BOOL "" FORCE)
set(ENABLE_STATIC ON CACHE BOOL "" FORCE)
set(ENABLE_SHARED OFF CACHE BOOL "" FORCE)
set(ENABLE_SRV OFF CACHE BOOL "" FORCE)
set(ENABLE_SNAPPY OFF CACHE BOOL "" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "" FORCE)
set(ENABLE_ICU OFF CACHE BOOL "" FORCE)
set(ENABLE_MONGODB_AWS_AUTH OFF CACHE BOOL "" FORCE)
set(ENABLE_UNINSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)
if(WIN32)
    set(ENABLE_SSL WINDOWS CACHE STRING "" FORCE)
    set(ENABLE_SASL SSPI CACHE STRING "" FORCE)
else()
    set(ENABLE_SSL OFF CACHE STRING "" FORCE)
    set(ENABLE_SASL OFF CACHE STRING "" FORCE)
endif()

FetchContent_Declare(
    mongo-c-driver
    GIT_REPOSITORY https://github.com/mongodb/mongo-c-driver.git
    GIT_TAG 1.28.1
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(mongo-c-driver)

add_library(tmf_logic STATIC src/logic.cpp)
target_include_directories(tmf_logic PUBLIC src)
target_link_libraries(tmf_logic PUBLIC nlohmann_json::nlohmann_json)

add_executable(tmf_logic_tests tests/logic_tests.cpp)
target_link_libraries(tmf_logic_tests PRIVATE tmf_logic)

add_executable(tmf_forum_backend
    src/main.cpp
    src/controller.cpp
    src/mongo_pool.cpp
    src/http_get.cpp
)
target_link_libraries(tmf_forum_backend PRIVATE tmf_logic httplib::httplib)
target_compile_definitions(tmf_forum_backend PRIVATE MONGOC_STATIC BSON_STATIC)
if(TARGET mongoc_static)
    target_link_libraries(tmf_forum_backend PRIVATE mongoc_static)
elseif(TARGET mongo::mongoc_static)
    target_link_libraries(tmf_forum_backend PRIVATE mongo::mongoc_static)
else()
    message(FATAL_ERROR "mongo-c-driver static target was not created")
endif()
if(WIN32)
    target_link_libraries(tmf_forum_backend PRIVATE ws2_32 winhttp)
endif()

enable_testing()
add_test(NAME logic COMMAND tmf_logic_tests)
