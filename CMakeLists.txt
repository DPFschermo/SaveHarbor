cmake_minimum_required(VERSION 3.10)
project(SaveHarbor)

set(CMAKE_CXX_STANDARD 17)

include_directories(include)

set(SOURCES
    src/main.cpp
    src/xbox360/stfs_scanner.cpp
    src/xbox360/stfs_parser.cpp
    src/xbox360/xtaf_parser.cpp
)

add_executable(saveharbor ${SOURCES})

if(WIN32)
    target_compile_definitions(saveharbor PRIVATE _CRT_SECURE_NO_WARNINGS)
    set_property(TARGET saveharbor PROPERTY
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
