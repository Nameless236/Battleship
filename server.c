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
