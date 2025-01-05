#include "client.h"
#include "pipe.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdlib.h>

#define SERVER_FIFO_PATH "/tmp/server_fifo"

void run_client() {
    const char *server_fifo = "/tmp/server_fifo";
    char client_read_fifo[256], client_write_fifo[256];
    snprintf(client_read_fifo, sizeof(client_read_fifo), "/tmp/client_%d_read", getpid());
    snprintf(client_write_fifo, sizeof(client_write_fifo), "/tmp/client_%d_write", getpid());

    unlink(client_read_fifo); // Remove old FIFOs if they exist
    unlink(client_write_fifo);

    pipe_init(client_read_fifo); // Create new FIFOs
    pipe_init(client_write_fifo);

    char connect_msg[256];
    snprintf(connect_msg, sizeof(connect_msg), "CONNECT %d", getpid());
    send_message(server_fifo, connect_msg);

    char buffer[1024];
    while (1) {
        printf("Enter command (PLACE x y length orientation, ATTACK x y, QUIT): ");
        fgets(buffer, sizeof(buffer), stdin);
        buffer[strcspn(buffer, "\n")] = '\0';

        send_message(server_fifo, buffer);
        receive_message(client_read_fifo, buffer, sizeof(buffer));
        printf("Server response: %s\n", buffer);

        if (strcmp(buffer, "GAME_OVER") == 0 || strcmp(buffer, "DISCONNECTED") == 0) {
            break;
        }
    }

    pipe_destroy(client_read_fifo);
    pipe_destroy(client_write_fifo);
}

void send_message(const char *path, const char *message) {
    // Otvorenie FIFO na zápis
    int fd = pipe_open_write(path);
    if (fd == -1) {
        perror("Failed to open FIFO for writing");
        exit(EXIT_FAILURE);
    }

    // Odoslanie správy
    if (write(fd, message, strlen(message) + 1) == -1) {
        perror("Failed to write to FIFO");
    }

    // Uzavretie FIFO
    pipe_close(fd);
    printf("Message sent to %s: %s\n", path, message);
}

void receive_message(const char *path, char *buffer, size_t buffer_size) {
    // Otvorenie FIFO na čítanie
    int fd = pipe_open_read(path);
    if (fd == -1) {
        perror("Failed to open FIFO for reading");
        exit(EXIT_FAILURE);
    }

    // Čítanie správy
    memset(buffer, 0, buffer_size); // Vyčistenie bufferu
    if (read(fd, buffer, buffer_size) > 0) {
        printf("Message received from %s: %s\n", path, buffer);
    } else {
        perror("Failed to read from FIFO");
    }

    // Uzavretie FIFO
    pipe_close(fd);
}

int connect_to_server(const char *server_fifo, int port) {
    char client_pipe_path[256];
    int client_id = getpid(); // Použitie PID na identifikáciu klienta

    // Vytvorenie klientského FIFO
    snprintf(client_pipe_path, sizeof(client_pipe_path), "/tmp/client_%d", client_id);
    pipe_init(client_pipe_path);

    // Poslanie požiadavky na pripojenie
    char connect_msg[256];
    snprintf(connect_msg, sizeof(connect_msg), "CONNECT %d", client_id);
    send_message(server_fifo, connect_msg);

    // Otvorenie klientského FIFO pre odpovede od servera
    char buffer[1024];
    receive_message(client_pipe_path, buffer, sizeof(buffer));

    if (strncmp(buffer, "ACCEPT", 6) != 0) {
        fprintf(stderr, "Server rejected the connection.\n");
        pipe_destroy(client_pipe_path);
        return -1;
    }

    return 0; // Pripojenie úspešné
}

void play_game(const char *server_fifo, const char *client_fifo) {
    char buffer[1024];
    char command[1024];
    int running = 1;

    while (running) {
        printf("Enter command (PLACE x y length orientation, ATTACK x y, QUIT): ");
        fgets(command, sizeof(command), stdin);

        // Odstránenie nového riadku zo vstupu
        command[strcspn(command, "\n")] = '\0';

        send_message(server_fifo, command);
        receive_message(client_fifo, buffer, sizeof(buffer));

        printf("Server response: %s\n", buffer);

        if (strcmp(buffer, "GAME_OVER") == 0 || strcmp(buffer, "DISCONNECTED") == 0) {
            running = 0;
        }
    }
}
