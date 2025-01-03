#include "server.h"
#include "pipe.h"
#include "game-logic.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define SERVER_FIFO_PATH "server_fifo"
#define CLIENT_FIFO_TEMPLATE "client_fifo_%d"
#define MAX_CLIENTS 2

void handle_client(int client_socket) {
    GameBoard board;
    initialize_board(&board);
    char buffer[1024];
    char response[1024];

    // Dynamicky generované cesty pre FIFO
    char client_read_path[256], client_write_path[256];
    snprintf(client_read_path, sizeof(client_read_path), "/tmp/client_%d_read", client_socket);
    snprintf(client_write_path, sizeof(client_write_path), "/tmp/client_%d_write", client_socket);

    // Inicializácia dátovodov
    pipe_init(client_read_path);
    pipe_init(client_write_path);

    int read_fd = pipe_open_read(client_read_path);
    int write_fd = pipe_open_write(client_write_path);

    if (read_fd == -1 || write_fd == -1) {
        perror("Failed to open client FIFOs");
        pipe_destroy(client_read_path);
        pipe_destroy(client_write_path);
        return;
    }

    while (1) {
        // Prijatie správy od klienta
        receive_message(client_read_path, buffer, sizeof(buffer));

        // Spracovanie príkazu
        char command[32];
        int x, y, length;
        char orientation;
        sscanf(buffer, "%s", command);

        if (strcmp(command, "PLACE") == 0) {
            sscanf(buffer, "%s %d %d %d %c", command, &x, &y, &length, &orientation);
            int result = place_ship(&board, x, y, length, orientation);
            snprintf(response, sizeof(response), "PLACE_RESPONSE %d", result);
            send_message(client_write_path, response);
        } else if (strcmp(command, "ATTACK") == 0) {
            sscanf(buffer, "%s %d %d", command, &x, &y);
            int result = attack(&board, x, y);
            snprintf(response, sizeof(response), "ATTACK_RESPONSE %d", result);
            send_message(client_write_path, response);

            if (is_game_over(&board)) {
                send_message(client_write_path, "GAME_OVER");
                break;
            }
        } else if (strcmp(command, "QUIT") == 0) {
            break;
        }
    }

    // Čistenie
    pipe_close(read_fd);
    pipe_close(write_fd);
    pipe_destroy(client_read_path);
    pipe_destroy(client_write_path);
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
    if (sem_init(&fifo_semaphore, 0, 1) != 0) {
        perror("Failed to initialize semaphore");
        exit(1);
    }

    // Inicializácia FIFO servera
    pipe_init(SERVER_FIFO_PATH);
    printf("Server FIFO created at path: %s\n", SERVER_FIFO_PATH);
}



void accept_connection(void) {
    char client_fifo[256];
    char buffer[1024];
    int client_id = 0;

    while (1) {
        sem_wait(&fifo_semaphore);
        int fd = pipe_open_read(SERVER_FIFO_PATH);
        sem_post(&fifo_semaphore);

        if (fd == -1) {
            perror("Failed to open server FIFO for reading");
            continue;
        }

        // Čítanie mena klientského FIFO
        if (read(fd, buffer, sizeof(buffer)) > 0) {
            snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, client_id++);
            printf("Client connected: %s\n", buffer);

            // Vytvorenie vlákna pre klienta
            pthread_t client_thread;
            char *fifo_name = strdup(client_fifo); // Prenos mena FIFO do vlákna

            if (pthread_create(&client_thread, NULL, (void *)handle_client, (void *)(intptr_t)client_id) != 0) {
                perror("Failed to create thread");
            }

            pthread_detach(client_thread);
        }

        pipe_close(fd);
    }
}

void receive_message(const char *path, char *buffer, size_t buffer_size) {
    pipe_init(path);

    int fd = pipe_open_read(path);
    if (fd == -1) {
        perror("Failed to open FIFO for reading");
        exit(EXIT_FAILURE);
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
    // Inicializácia FIFO, ak ešte neexistuje
    pipe_init(path);

    // Otvorenie FIFO na zápis
    int fd = pipe_open_write(path);
    if (fd == -1) {
        perror("Failed to open FIFO for writing");
        exit(EXIT_FAILURE);
    }

    // Zápis správy do FIFO
    if (write(fd, message, strlen(message) + 1) == -1) {
        perror("Failed to write to FIFO");
    }

    // Uzavretie FIFO
    pipe_close(fd);
    printf("Message sent to %s: %s\n", path, message);
}