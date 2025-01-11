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

typedef struct {
    int id;
    char name[20];
    int size;
} Ship;

typedef struct {
    Ship ships[5];
} Fleet;


void initialize_fleet(Fleet *fleet);

// Inicializuje hernú mriežku
void initialize_board(GameBoard *board);

// Umiestni loď na mriežku
int place_ship_c(GameBoard *board, int x, int y, int length, char orientation);

// Simuluje útok na konkrétnu pozíciu
int attack(GameBoard *board, int x, int y);

// Overí, či sú všetky lode zničené
int is_game_over(GameBoard *board);

void print_board(GameBoard *board);

void print_boards(GameBoard *my_board, GameBoard *enemy_board);

int place_ship_from_fleet(GameBoard *board, int x, int y, Ship *ship, char orientation);

void print_fleet(Fleet *fleet, int ships);