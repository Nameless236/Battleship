#pragma once
#include <stddef.h>

// Run the client process
void run_client(void);

// Play the game with the server
void play_game(const char *server_fifo, const char *client_fifo);

// Connect the client to the server using FIFOs
int connect_to_server(const char *server_fifo_path, const char *client_fifo_path);

// Handle sending a move to the server
int send_move(const char *server_fifo, const char *move);

// Handle receiving a response from the server
int receive_response(const char *client_fifo, char *buffer, size_t buffer_size);
