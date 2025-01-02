cmake_minimum_required(VERSION 3.0)

project(
        Battleship
        VERSION 1.0
        DESCRIPTION "Battleship game"
        LANGUAGES C
)

add_executable(battleship main.c)
target_sources(battleship PRIVATE main.c)