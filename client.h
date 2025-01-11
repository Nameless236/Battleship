#pragma once
#include <stddef.h>
#include <semaphore.h>
#include "game-logic.h"
#include <stdbool.h>
#include <stdatomic.h> // For atomic_bool

// Struct Definitions
typedef struct {
    GameBoard my_board;
    GameBoard enemy_board;
    Fleet fleet;
    int ships_to_place;
    atomic_bool game_over; // Atomic flag to signal game termination
    int board_ready;
} ClientGameState;

typedef struct {
    int write_fd;
    int read_fd;
    int client_id;
    ClientGameState *game_state;
    sem_t *sem_command;  // For sending commands
    sem_t *sem_response; // For reading responses
} ThreadArgs;

void handle_game_over(const char *message);

void send_board_to_server(int write_fd, int client_id, GameBoard *board);

int run_client(int argc, char *argv[]);

void initialize_client_game_state(ClientGameState *state) ;

void create_server_process(const char *server_name);

void setup_communication(const char *server_name, ThreadArgs *args);

void cleanup_resources(ThreadArgs *args);

void connect_to_server(ThreadArgs *args);

void handle_client_threads(ThreadArgs *args);

void *handle_updates(void *arg);

void *handle_commands(void *arg);

void handle_gameplay_commands(ThreadArgs *args);

void place_ships(ClientGameState *game_state, ThreadArgs *args) ;

void process_server_message(ThreadArgs *args, const char *message);