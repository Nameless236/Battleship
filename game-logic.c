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

int place_ship_c(GameBoard *board, int x, int y, int length, char orientation) {
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
    // Check if the coordinates are within bounds
    if (x < 0 || x >= BOARD_SIZE || y < 0 || y >= BOARD_SIZE) {
        printf("Invalid coordinates (%d, %d). Out of bounds.\n", x, y);
        return -1; // Invalid attack
    }

    // Check if the cell has already been attacked
    if (board->grid[y][x] == 2 || board->grid[y][x] == 3) {
        printf("Cell (%d, %d) has already been attacked.\n", x, y);
        return -1; // Already attacked
    }

    // Determine if it's a hit or miss
    if (board->grid[y][x] == 1) { // 1 indicates a ship is present
        board->grid[y][x] = 2;    // Mark as hit
        printf("Hit at (%d, %d)!\n", x, y);
        if (is_game_over(board)) {
            return 2; // Game over
        }
        return 1; // Hit
    } else { // Empty cell
        board->grid[y][x] = 3; // Mark as miss
        printf("Miss at (%d, %d).\n", x, y);
        return 0; // Miss
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


void print_boards(GameBoard *my_board, GameBoard *enemy_board) {
    printf("   Vaša mapa:                          Superova mapa:\n");
    printf("   ");

    for (int j = 0; j < BOARD_SIZE; j++) {
        printf(" %d ", j);
    }
    printf("          ");
    for (int j = 0; j < BOARD_SIZE; j++) {
        printf(" %d ", j);
    }
    printf("\n");

    for (int i = 0; i < BOARD_SIZE; i++) {
        printf(" %d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            char cell = my_board->grid[i][j];
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

        printf("       ");

        printf(" %d ", i);
        for (int j = 0; j < BOARD_SIZE; j++) {
            char cell = enemy_board->grid[i][j];
            if (cell == 0 || cell == 1) {
                printf("[ ]"); // Voda alebo neodhalená loď
            } else if (cell == 2) {
                printf("[X]"); // Zásah
            } else if (cell == 3) {
                printf("[~]"); // Minutie
            }
        }
        printf("\n");
    }
}

void initialize_fleet(Fleet *fleet) {
    fleet->ships[0] = (Ship){1, "Carrier", 5};
    fleet->ships[1] = (Ship){2, "Battleship", 4};
    fleet->ships[2] = (Ship){3, "Destroyer", 3};
    fleet->ships[3] = (Ship){4, "Submarine", 3};
    fleet->ships[4] = (Ship){5, "Patrol Boat", 2};
}

int place_ship_from_fleet(GameBoard *board, int x, int y, Ship *ship, char orientation) {
    return place_ship_c(board, x, y, ship->size, orientation);
}

void print_fleet(Fleet *fleet, int remaining_ships) {
    printf("\nRemaining Fleet:\n");
    printf("------------------------\n");
    printf(" %-11s | %-4s\n", "Name", "Size");
    printf("------------------------\n");
    for (int i = 5 - remaining_ships; i < 5; i++) {
        Ship *ship = &fleet->ships[i];
        printf(" %-11s | %-4d\n", ship->name, ship->size);
    }
    printf("------------------------\n");
}


