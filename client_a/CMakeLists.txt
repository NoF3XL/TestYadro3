cmake_minimum_required(VERSION 3.16)
project(AccelerometerClientA)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(client_a client_a.cpp)

target_include_directories(client_a PRIVATE ${CMAKE_SOURCE_DIR}/..)

include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
    DOWNLOAD_NO_EXTRACT TRUE
)
FetchContent_MakeAvailable(json)

target_include_directories(client_a PRIVATE ${json_SOURCE_DIR}/single_include)