#pragma once

#include <stddef.h>

// Štruktúra hernej mriežky
typedef struct {
    int grid[10][10];   // Player's ships and enemy attacks
    int ships_remaining;
} GameBoard;

typedef struct {
    GameBoard ships;
    GameBoard attacks;
} Player;



// Inicializuje hernú mriežku
void initialize_board(GameBoard *board);

// Umiestni loď na mriežku
int place_ship(GameBoard *board, int x, int y, int length, char orientation);

// Simuluje útok na konkrétnu pozíciu
int attack(GameBoard *board, int x, int y);

// Overí, či sú všetky lode zničené
int is_game_over(GameBoard *board);

void print_board(GameBoard *board);

void print_boards(GameBoard *my_board, GameBoard *enemy_board);