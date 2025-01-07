#pragma once

#include <pthread.h>
#include "game-logic.h"

// Mutex for synchronizing access to FIFOs
extern pthread_mutex_t fifo_mutex;

typedef struct {
    GameBoard *board_players[2];    // Herné mriežky hráčov
    int player_turn;               // ID hráča, ktorý je na ťahu (1 alebo 2)
    int client_id_1;
    int client_id_2;
} GameData;

// Structure to store client-specific information
typedef struct {
    int client_id; // Unique ID for the client
    GameData *game_data; // Pointer to shared game data
} ClientInfo;

// Initialize the server resources
void initialize_server(void);

// Run the server loop to handle clients
void run_server(void);

// Handle communication with a single client (thread function)
void *handle_client(void *arg);
