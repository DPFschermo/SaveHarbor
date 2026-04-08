cmake_minimum_required(VERSION 3.10)
project(SaveHarbor)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

add_executable(saveharbor
    src/main.cpp
    src/xbox360/stfs_scanner.cpp
    src/xbox360/stfs_parser.cpp
    src/xbox360/xtaf_parser.cpp
)
