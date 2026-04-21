cmake_minimum_required(VERSION 3.20)
project(DBDReboot LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(dbd_shared INTERFACE)
target_include_directories(dbd_shared INTERFACE
    shared/include
    server/include
)

add_executable(dbd_server
    server/src/main.cpp
    server/src/world_state.cpp
    server/src/world_bootstrap.cpp
    server/src/server_simulation.cpp
)
target_link_libraries(dbd_server PRIVATE dbd_shared)
