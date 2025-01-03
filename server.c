#include "server.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

void handle_client(int client_socket) {
    GameBoard board;
    initialize_board(&board);
    char buffer[1024];
    char response[1024];

    // Create client-specific pipes
    char client_read_path[256], client_write_path[256];
    snprintf(client_read_path, sizeof(client_read_path), "/tmp/client_%d_read", client_socket);
    snprintf(client_write_path, sizeof(client_write_path), "/tmp/client_%d_write", client_socket);

    // Initialize pipes
    pipe_init(client_read_path);
    pipe_init(client_write_path);

    int read_fd = pipe_open_read(client_read_path);
    int write_fd = pipe_open_write(client_write_path);

    if (read_fd == -1 || write_fd == -1) {
        pipe_destroy(client_read_path);
        pipe_destroy(client_write_path);
        return;
    }

    while (1) {
        // Receive message from client
        receive_message(read_fd, buffer, sizeof(buffer));

        // Parse command
        char command[32];
        int x, y, length;
        char orientation;
        sscanf(buffer, "%s", command);

        if (strcmp(command, "PLACE") == 0) {
            sscanf(buffer, "%s %d %d %d %c", command, &x, &y, &length, &orientation);
            int result = place_ship(&board, x, y, length, orientation);
            snprintf(response, sizeof(response), "PLACE_RESPONSE %d", result);
            send_message(write_fd, response);
        }
        else if (strcmp(command, "ATTACK") == 0) {
            sscanf(buffer, "%s %d %d", command, &x, &y);
            int result = attack(&board, x, y);
            snprintf(response, sizeof(response), "ATTACK_RESPONSE %d", result);
            send_message(write_fd, response);

            if (is_game_over(&board)) {
                send_message(write_fd, "GAME_OVER");
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

void broadcast_message(const char *message, int exclude_client) {
    char pipe_path[256];
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (i != exclude_client) {
            // Create path for client's write pipe
            snprintf(pipe_path, sizeof(pipe_path), "/tmp/client_%d_write", i);

            // Try to open pipe for writing
            int fd = pipe_open_write(pipe_path);
            if (fd != -1) {
                // Send message using the provided send_message function
                send_message(fd, message);
                pipe_close(fd);
            }
        }
    }
}

#define SERVER_FIFO_PATH "server_fifo"
#define CLIENT_FIFO_TEMPLATE "client_fifo_%d"

void initialize_server(void) {
  if (sem_init(&fifo_semaphore, 0, 1) != 0) {
    perror("Failed to initialize semaphore");
    exit(1);
  }
  // Vytvoríme FIFO s názvom server_fifo
  if (mkfifo(SERVER_FIFO_PATH, 0666) == -1) {
      perror("Failed to create server FIFO");
      exit(1);
  }

  printf("Server FIFO created at path: %s\n", SERVER_FIFO_PATH);
}


void accept_connection(void) {
  char client_fifo[256];
  char buffer[1024];
  int client_id = 0;

  while (1) {
    sem_wait(&fifo_semaphore);
    int fd = open(SERVER_FIFO_PATH, O_RDONLY);
    sem_post(&fifo_semaphore);

    if (fd == -1) {
      perror("Failed to open server FIFO for reading");
      continue;
    }

    // Čítanie mena klientského FIFO
    if (read(fd, buffer, sizeof(buffer)) > 0) {
      snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, client_id++);
      printf("Client connected: %s\n", buffer);

      // Vytvorenie vlákna na obsluhu klienta
      pthread_t client_thread;
      char *fifo_name = strdup(client_fifo); // Prenos mena FIFO do vlákna

      if (pthread_create(&client_thread, NULL, handle_client, fifo_name) != 0) {
        perror("Failed to create thread");
      }

      pthread_detach(client_thread); // Automatické čistenie vlákna po ukončení
    }
    close(fd);
    }
}