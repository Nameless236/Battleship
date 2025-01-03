#include "game-logic.h"

int attac(GameBoard *board, int x, int y) {
    if (x < 0 || x >= 10 || y < 0 || y >= 10) {
        return -1; // Neplatná pozícia
    }

    if (board->cells[x][y] == 1) {
        board->cells[x][y] = 2; // Zásah
        return 1;
    } else if (board->cells[x][y] == 0) {
        board->cells[x][y] = 3; // Minutie
        return 0;
    } else {
        return -2; // Už tam bolo zasiahnuté
    }
}

int is_game_over(GameBoard *board) {
    for (int i = 0; i < 10; i++) {
        for (int j = 0; j < 10; j++) {
            if (board->cells[i][j] == 1) {
                return 0; // Ešte nie sú zničené všetky lode
            }
        }
    }

    return 1; // Všetky lode boli zničené
}