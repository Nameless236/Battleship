#include "game-logic.h"

#define BOARD_SIZE 10

void initialize_board(GameBoard *board) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            board->grid[i][j] = 0;  // Water
        }
    }
}