#include "server.h"
#include "pipe.h"
#include "game-logic.h"
#include "client.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <semaphore.h>
#include <pthread.h>

#define SERVER_FIFO_PATH "/tmp/server_fifo"
#define CLIENT_FIFO_TEMPLATE "client_fifo_%d"
#define MAX_CLIENTS 2

sem_t fifo_semaphore; // Semaphore for thread-safe access
ClientData *connected_players[MAX_CLIENTS]; // Array to store connected players
int player_count = 0; // Counter for connected players
GameBoard game_board; // Game board

void initialize_game() {
    // Initialize the game board
    initialize_board(&game_board);
    printf("Game board initialized.\n");

    // Notify both players that the game is starting
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (connected_players[i] != NULL) {
            char message[256];
            snprintf(message, sizeof(message), "GAME_START Player %d", i + 1);
            send_message(connected_players[i]->fifo_path, message);
        }
    }

    printf("Game is starting with players: %d and %d\n", connected_players[0]->client_id, connected_players[1]->client_id);
}

void handle_client(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    char buffer[1024];
    char response[1024];

    printf("Handling client ID: %d\n", client_data->client_id);

    while (1) {
        receive_message(client_data->fifo_path, buffer, sizeof(buffer));

        if (strlen(buffer) == 0) {
            printf("No command received. Closing client connection.\n");
            break;
        }

        char command[32];
        sscanf(buffer, "%s", command);

        if (strcmp(command, "QUIT") == 0) {
            snprintf(response, sizeof(response), "DISCONNECTED");
            send_message(client_data->fifo_path, response);
            break;
        } else {
            snprintf(response, sizeof(response), "UNKNOWN_COMMAND");
            send_message(client_data->fifo_path, response);
        }
    }

    free(client_data); // Free the client data
    printf("Client connection closed.\n");
}

void accept_connection(void) {
    char buffer[1024];

    while (1) {
        printf("Waiting for client connection...\n");

        sem_wait(&fifo_semaphore); // Wait on the semaphore
        int fd = pipe_open_read(SERVER_FIFO_PATH);
        sem_post(&fifo_semaphore); // Signal the semaphore

        if (fd == -1) {
            perror("Failed to open server FIFO for reading");
            continue;
        }

        if (read(fd, buffer, sizeof(buffer)) > 0) {
            printf("Client connected: %s\n", buffer);

            ClientData *client_data = malloc(sizeof(ClientData));
            if (!client_data) {
                perror("Failed to allocate memory for client data");
                continue;
            }

            snprintf(client_data->fifo_path, sizeof(client_data->fifo_path), CLIENT_FIFO_TEMPLATE, player_count);
            client_data->client_id = player_count;

            connected_players[player_count] = client_data; // Store player data
            player_count++;

            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, (void *)handle_client, (void *)client_data) != 0) {
                perror("Failed to create thread");
                free(client_data);
                continue;
            }
            pthread_detach(client_thread);
            puts("Client thread created.");

            // Check if two players are connected
            if (player_count == MAX_CLIENTS) {
                initialize_game();
                break; // Exit the loop once the game starts
            }
        }

        pipe_close(fd);
    }
}

void initialize_server(void) {
    if (sem_init(&fifo_semaphore, 0, 1) == -1) { // Binary semaphore with initial value 1
        perror("Failed to initialize semaphore");
        exit(EXIT_FAILURE);
    }

    unlink(SERVER_FIFO_PATH); // Remove old FIFO if it exists

    if (mkfifo(SERVER_FIFO_PATH, 0666) == -1) { // Create server FIFO
        perror("Failed to create server FIFO");
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO created at path: %s\n", SERVER_FIFO_PATH);
}

void run_server() {
    initialize_server();

    while (1) {
        accept_connection(); // Wait for clients and handle connections
    }

    sem_destroy(&fifo_semaphore); // Clean up semaphore when server shuts down
}
