#include "server.h"
#include "pipe.h"
#include "game-logic.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <dispatch/dispatch.h>

#define SERVER_FIFO_PATH "/tmp/server_fifo"
#define CLIENT_FIFO_TEMPLATE "client_fifo_%d"
#define MAX_CLIENTS 2

dispatch_semaphore_t fifo_semaphore = NULL;

void run_server() {
    initialize_server();

    while (1) {
        printf("Waiting for client connection...\n");
        accept_connection(); // Spracovanie pripojení klientov
    }
}

void handle_client(void *arg) {
    ClientData *client_data = (ClientData *)arg;
    char buffer[1024];
    char response[1024];
    GameBoard board;

    initialize_board(&board);

    printf("Handling client ID: %d\n", client_data->client_id);

    while (1) {
        printf("Attempting to read message from %s\n", client_data->fifo_path);
        receive_message(client_data->fifo_path, buffer, sizeof(buffer));

        if (strlen(buffer) == 0) {
            printf("No command received. Closing client connection.\n");
            break;
        }

        char command[32];
        int x, y, length;
        char orientation;

        sscanf(buffer, "%s", command);
        printf("Received command: %s\n", command);

        if (strcmp(command, "PLACE") == 0) {
            sscanf(buffer, "%s %d %d %d %c", command, &x, &y, &length, &orientation);
            int result = place_ship(&board, x, y, length, orientation);
            snprintf(response, sizeof(response), "PLACE_RESPONSE %d", result);
        } else if (strcmp(command, "ATTACK") == 0) {
            sscanf(buffer, "%s %d %d", command, &x, &y);
            int result = attack(&board, x, y);
            snprintf(response, sizeof(response), "ATTACK_RESPONSE %d", result);
            if (is_game_over(&board)) {
                snprintf(response, sizeof(response), "GAME_OVER");
                break;
            }
        } else if (strcmp(command, "QUIT") == 0) {
            snprintf(response, sizeof(response), "DISCONNECTED");
            break;
        } else {
            snprintf(response, sizeof(response), "UNKNOWN_COMMAND");
        }

        printf("Attempting to send message to %s: %s\n", client_data->fifo_path, response);
        send_message(client_data->fifo_path, response);
    }

    free(client_data); // Free the client data
    printf("Client connection closed.\n");
}


void broadcast_message(const char *message, int exclude_client) {
    char pipe_path[256];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != exclude_client) {
            snprintf(pipe_path, sizeof(pipe_path), "/tmp/client_%d_write", i);

            // Otvorenie FIFO na zápis
            int fd = pipe_open_write(pipe_path);
            if (fd != -1) {
                send_message(pipe_path, message);
                pipe_close(fd);
            }
        }
    }
}

void initialize_server(void) {
    fifo_semaphore = dispatch_semaphore_create(1);
    if (fifo_semaphore == NULL) {
        perror("Failed to initialize semaphore");
        exit(EXIT_FAILURE);
    }

    // Odstránenie starého FIFO, ak existuje
    unlink(SERVER_FIFO_PATH);

    // Vytvorenie FIFO
    if (mkfifo(SERVER_FIFO_PATH, 0666) == -1) {
        perror("Failed to create server FIFO");
        exit(EXIT_FAILURE);
    }

    printf("Server FIFO created at path: %s\n", SERVER_FIFO_PATH);
}

void accept_connection(void) {
    char buffer[1024];
    int client_id = 0;

    while (1) {
        printf("Waiting for client connection...\n");
        dispatch_semaphore_wait(fifo_semaphore, DISPATCH_TIME_FOREVER);
        int fd = pipe_open_read(SERVER_FIFO_PATH);
        dispatch_semaphore_signal(fifo_semaphore);

        if (fd == -1) {
            perror("Failed to open server FIFO for reading");
            continue;
        }

        if (read(fd, buffer, sizeof(buffer)) > 0) {
            printf("Client connected: %s\n", buffer);

            char client_fifo[256];
            snprintf(client_fifo, sizeof(client_fifo), "/tmp/client_%d", client_id);

            pipe_init(client_fifo);

            ClientData *client_data = malloc(sizeof(ClientData));
            if (!client_data) {
                perror("Failed to allocate memory for client data");
                continue;
            }

            strncpy(client_data->fifo_path, client_fifo, sizeof(client_data->fifo_path));
            client_data->client_id = client_id;

            pthread_t client_thread;
            if (pthread_create(&client_thread, NULL, (void *)handle_client, (void *)client_data) != 0) {
                perror("Failed to create thread");
                free(client_data);
            }
            pthread_detach(client_thread);
            client_id++;
        }

        pipe_close(fd);
    }
    unlink(SERVER_FIFO_PATH);
}


void receive_message(const char *path, char *buffer, size_t buffer_size) {
    printf("Attempting to read message from %s\n", path);

    int fd = pipe_open_read(path);
    if (fd == -1) {
        perror("Failed to open FIFO for reading");
        return; // Avoid crashing the server
    }

    memset(buffer, 0, buffer_size);
    if (read(fd, buffer, buffer_size) > 0) {
        printf("Message received from %s: %s\n", path, buffer);
    } else {
        perror("Failed to read from FIFO");
    }

    pipe_close(fd);
}

void send_message(const char *path, const char *message) {
    printf("Attempting to send message to %s: %s\n", path, message);

    int fd = pipe_open_write(path);
    if (fd == -1) {
        perror("Failed to open FIFO for writing");
        return; // Avoid crashing the server
    }

    if (write(fd, message, strlen(message) + 1) == -1) {
        perror("Failed to write to FIFO");
    } else {
        printf("Message sent to %s: %s\n", path, message);
    }

    pipe_close(fd);
}
