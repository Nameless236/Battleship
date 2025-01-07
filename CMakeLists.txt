cmake_minimum_required(VERSION 3.0)

project(
    Battleship
    VERSION 1.0
    DESCRIPTION "Battleship game"
    LANGUAGES C
)

# Common library for shared functionality
add_library(common pipe.c communication.c game-logic.c)

# Client executable
add_executable(client client.c main.c)
target_link_libraries(client PRIVATE common)
target_compile_definitions(client PRIVATE CLIENT)

# Server executable
add_executable(server server.c main.c)
target_link_libraries(server PRIVATE common Threads::Threads)
target_compile_definitions(server PRIVATE SERVER)

# Find pthread for server
find_package(Threads REQUIRED)
