#include "game-logic.h"
#include <stdio.h>
#include <stdlib.h>

#define BOARD_SIZE 10

void initialize_board(GameBoard *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->grid[i][j] = 0;  // Water
        }
    }
    board->ships_remaining = 0;
}

int place_ship(GameBoard *board, int x, int y, int length, char orientation) {
    // Validate coordinates and ship length
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE || length < 2 || length > 5) {
        return 0; // Invalid input
    }

    // Check boundaries and availability of space based on orientation
    if (orientation == 'H') {
        // Check if the ship fits horizontally
        if (x + length > BOARD_SIZE) {
            return 0; // Out of bounds
        }
        
        
        // Validate if the space and surroundings are free
        for (int i = x - 1; i <= x + length; i++) {
            for (int j = y - 1; j <= y + 1; j++) {
                if(i >=0 && i<=9 && j>=0 && j<=9) {
                    if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                        if (board->grid[j][i] != 0) {
                            return 0; // Space is occupied or adjacent to another ship
                        }
                    }
                }
            }
        }

        // Place the ship horizontally
        for (int i = x; i < x + length; i++) {
            board->grid[y][i] = 1; // Mark ship parts
        }
    } else if (orientation == 'V') {
        // Check if the ship fits vertically
        if (y + length > BOARD_SIZE) {
            return 0; // Out of bounds
        }

        // Validate if the space and surroundings are free
        for (int i = x - 1; i <= x + 1; i++) {
            for (int j = y - 1; j <= y + length; j++) {
                if(i >=0 && i<=9 && j>=0 && j<=9) {
                    if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                        if (board->grid[j][i] != 0) {
                            return 0; // Space is occupied or adjacent to another ship
                        }
                    }
                }
            }
        }

        // Place the ship vertically
        for (int j = y; j < y + length; j++) {
            board->grid[j][x] = 1; // Mark ship parts
        }
    } else {
        return 0; // Invalid orientation
    }

    // Increment the count of placed ships
    board->ships_remaining++;
    return 1; // Ship placed successfully
}

int attack(GameBoard *board, int x, int y) {
    if (x < 0 || x >= 10 || y < 0 || y >= 10) {
        return -1; // Neplatná pozícia
    }

    if (board->grid[x][y] == 1) {
        board->grid[x][y] = 2; // Zásah
        return 1;
    } else if (board->grid[x][y] == 0) {
        board->grid[x][y] = 3; // Minutie
        return 0;
    } else {
        return -2; // Už tam bolo zasiahnuté
    }
}

int is_game_over(GameBoard *board) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            if (board->grid[i][j] == 1) {
                return 0; // Ešte nie sú zničené všetky lode
            }
        }
    }

    return 1; // Všetky lode boli zničené
}

void print_board(GameBoard *board) {
    printf("   ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf(" %d ", j);
    }
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        printf(" %d ", i);

        for (int j = 0; j < BOARD_SIZE; j++) {
            char cell = board->grid[i][j];
            // Ak je hodnota číslo, prevedieme na znak pre prehľadnosť
            if (cell == 0) {
                printf("[ ]"); // Voda
            } else if (cell == 1) {
                printf("[L]"); // Loď
            } else if (cell == 2) {
                printf("[X]"); // Zásah
            } else if (cell == 3) {
                printf("[~]"); // Minutie
            }
        }
        printf("\n");
    }
}
