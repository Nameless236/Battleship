cmake_minimum_required(VERSION 3.0)

project(
        Battleship
        VERSION 1.0
        DESCRIPTION "Battleship game"
        LANGUAGES C
)

# Klient
add_executable(client client.c pipe.c main.c)
target_compile_definitions(client PRIVATE CLIENT)

# Server
add_executable(server server.c pipe.c game-logic.c main.c client.c)
target_compile_definitions(server PRIVATE SERVER)

# Pridanie pthread pre server
find_package(Threads REQUIRED)
target_link_libraries(server PRIVATE Threads::Threads)
