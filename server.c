#include "server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


#define BUFFER_SIZE 1024
#define MAX_CLIENTS 10

typedef struct {
    int id;
    int pipe_read;   // Server reads from this pipe
    int pipe_write;  // Server writes to this pipe
    int is_active;
} ClientConnection;


void broadcast_message(ClientConnection** clients, int num_clients, const char* message, int exclude_client_id) {
    for (int i = 0; i < num_clients; i++) {
        if (clients[i] != NULL &&
            clients[i]->is_active &&
            clients[i]->id != exclude_client_id) {

            // Send message to each active client except the excluded one
            if (write(clients[i]->pipe_write, message, strlen(message)) == -1) {
                // Handle write error
                clients[i]->is_active = 0;
                close(clients[i]->pipe_read);
                close(clients[i]->pipe_write);
            }
        }
    }
}

void handle_client(int client_socket) {
    GameBoard board;
    initialize_board(&board);
    char buffer[1024];
    char response[1024];
    int bytes_read;

    // Create client-specific pipes for bidirectional communication
    char client_read_path[256], client_write_path[256];
    snprintf(client_read_path, sizeof(client_read_path), "/tmp/client_%d_read", client_socket);
    snprintf(client_write_path, sizeof(client_write_path), "/tmp/client_%d_write", client_socket);

    // Initialize pipes
    pipe_init(client_read_path);
    pipe_init(client_write_path);

    // Open pipes
    int read_fd = pipe_open_read(client_read_path);
    int write_fd = pipe_open_write(client_write_path);

    if (read_fd == -1 || write_fd == -1) {
        pipe_destroy(client_read_path);
        pipe_destroy(client_write_path);
        return;
    }

    while (1) {
        // Read client command
        bytes_read = read(read_fd, buffer, sizeof(buffer) - 1);
        if (bytes_read <= 0) {
            break;
        }
        buffer[bytes_read] = '\0';

        // Parse command
        char command[32];
        int x, y, length;
        char orientation;
        sscanf(buffer, "%s", command);

        // Process commands
        if (strcmp(command, "PLACE") == 0) {
            sscanf(buffer, "%s %d %d %d %c", command, &x, &y, &length, &orientation);
            int result = place_ship(&board, x, y, length, orientation);
            snprintf(response, sizeof(response), "PLACE_RESPONSE %d", result);
            write(write_fd, response, strlen(response));
        }
        else if (strcmp(command, "SHOOT") == 0) {
            sscanf(buffer, "%s %d %d", command, &x, &y);
            char result = board.grid[y][x];
            if (result == 'S') {
                board.grid[y][x] = 'H';
                board.ships_remaining--;
                snprintf(response, sizeof(response), "HIT %d %d", x, y);
            } else {
                board.grid[y][x] = 'M';
                snprintf(response, sizeof(response), "MISS %d %d", x, y);
            }
            write(write_fd, response, strlen(response));

            // Check for game end
            if (board.ships_remaining == 0) {
                write(write_fd, "GAME_OVER", 9);
                break;
            }
        }
        else if (strcmp(command, "QUIT") == 0) {
            break;
        }
    }

    // Cleanup
    pipe_close(read_fd);
    pipe_close(write_fd);
    pipe_destroy(client_read_path);
    pipe_destroy(client_write_path);
}

