cmake_minimum_required(VERSION 3.15)
project(tinydb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

add_library(tinydb STATIC
    src/storage.cpp
)

add_executable(tinydb_cli
    src/main.cpp
)
target_link_libraries(tinydb_cli PRIVATE tinydb)
