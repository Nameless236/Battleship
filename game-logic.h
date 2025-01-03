#pragma once

#include <stddef.h>

// Štruktúra hernej mriežky
typedef struct {
    int grid[10][10]; // 0 = prázdne, 1 = loď, 2 = zásah, 3 = minutie
} GameBoard;

// Štruktúra hráča
typedef struct {
    int id;             // ID hráča
    GameBoard board;    // Jeho herná mriežka
    int ships_remaining; // Počet zostávajúcich lodí
} Player;

// Štruktúra hry
typedef struct {
    Player players[2];  // Maximálne dvaja hráči
    int current_turn;   // ID hráča, ktorý je na ťahu
} GameState;

// Inicializuje hernú mriežku
void initialize_board(GameBoard *board);

// Umiestni loď na mriežku
int place_ship(GameBoard *board, int x, int y, int length, char orientation);

// Simuluje útok na konkrétnu pozíciu
int attack(GameBoard *board, int x, int y);

// Overí, či sú všetky lode zničené
int is_game_over(GameBoard *board);

