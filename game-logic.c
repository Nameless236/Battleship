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
    // Validate coordinates and length
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE || length < 2 || length > 5) {
        return 0;
    }

    // Check boundaries and space based on orientation
    if (orientation == 'H') {
        // Check if ship fits horizontally
        if (x + length > BOARD_SIZE) {
            return 0;
        }

        // Check if space is free and no adjacent ships
        for (int i = x - 1; i <= x + length; i++) {
            for (int j = y - 1; j <= y + 1; j++) {
                if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                    if (board->grid[i][j] != '0') {
                        return 0;
                    }
                }
            }
        }

        // Place ship horizontally
        for (int i = x; i < x + length; i++) {
            board->grid[i][y] = '1';
        }
    }
    else if (orientation == 'V') {
        // Check if ship fits vertically
        if (y + length > BOARD_SIZE) {
            return 0;
        }

        // Check if space is free and no adjacent ships
        for (int i = x - 1; i <= x + 1; i++) {
            for (int j = y - 1; j <= y + length; j++) {
                if (i >= 0 && i < BOARD_SIZE && j >= 0 && j < BOARD_SIZE) {
                    if (board->grid[i][j] != '0') {
                        return 0;
                    }
                }
            }
        }

        // Place ship vertically
        for (int j = y; j < y + length; j++) {
            board->grid[x][j] = '1';
        }
    }
    else {
        return 0; // Invalid orientation
    }

    board->ships_remaining++;
    return 1;
}
