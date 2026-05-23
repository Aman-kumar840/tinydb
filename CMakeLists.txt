cmake_minimum_required(VERSION 3.15)
project(tinydb LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 1. Include public headers
include_directories(include)

# 2. Build a static library with all core sources
add_library(tinydb STATIC
    src/storage.cpp
    src/table.cpp
    src/table_storage.cpp
)

# 3. CLI executable with main.cpp
add_executable(tinydb_cli
    src/main.cpp
)

# 4. Link static library to CLI
target_link_libraries(tinydb_cli PRIVATE tinydb)