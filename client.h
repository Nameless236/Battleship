#pragma once
#include <stddef.h>

#include "game-logic.h"

// Run the client process
void run_client(int argc, char *argv[]);

void create_server_process(const char *server_name);

void *handle_updates(void *arg);

void *handle_commands(void *arg);

void send_board_to_server(int write_fd, int client_id, GameBoard *board);