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
    server/src/tactical_state.cpp
)
target_link_libraries(dbd_server PRIVATE dbd_shared)
target_compile_definitions(dbd_server PRIVATE DBD_REBOOT_ROOT="${CMAKE_SOURCE_DIR}")

add_executable(dbd_play_session
    server/src/play_session.cpp
    server/src/world_state.cpp
    server/src/world_bootstrap.cpp
    server/src/server_simulation.cpp
    server/src/tactical_state.cpp
)
target_link_libraries(dbd_play_session PRIVATE dbd_shared)
target_compile_definitions(dbd_play_session PRIVATE DBD_REBOOT_ROOT="${CMAKE_SOURCE_DIR}")

add_executable(dbd_client WIN32
    client/src/native_client.cpp
)
target_compile_definitions(dbd_client PRIVATE DBD_REBOOT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(dbd_client PRIVATE dbd_shared user32 gdi32)

add_executable(dbd_client3d WIN32
    client/src/d3d_client.cpp
)
target_compile_definitions(dbd_client3d PRIVATE DBD_REBOOT_ROOT="${CMAKE_SOURCE_DIR}")
target_link_libraries(dbd_client3d PRIVATE dbd_shared user32 gdi32 d3d11 dxgi d3dcompiler)
