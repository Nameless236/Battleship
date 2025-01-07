#include "client.h"
#include "communication.h"
#include "pipe.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h> // For mmap
#include <sys/stat.h> // For mode constants

#define SERVER_READ_FIFO "/tmp/server_read_fifo"
#define SERVER_WRITE_FIFO "/tmp/server_write_fifo"
#define SEMAPHORE_FILE "/home/pastorek10/sem_connect" // Path to shared memory file
#define BUFFER_SIZE 1024

void run_client() {
    char buffer[BUFFER_SIZE];

    printf("Client: Connecting to server...\n");

    // Open FIFOs for communication
    int write_fd = pipe_open_write(SERVER_READ_FIFO);
    if (write_fd == -1) {
        perror("Client: Failed to open write FIFO");
        exit(EXIT_FAILURE);
    }

    int read_fd = pipe_open_read(SERVER_WRITE_FIFO);
    if (read_fd == -1) {
        perror("Client: Failed to open read FIFO");
        close(write_fd);
        exit(EXIT_FAILURE);
    }

    // Send connection request to the server
    const char *connect_message = "CONNECT";
    if (send_message(write_fd, connect_message) != 0) {
        perror("Client: Failed to send connection message");
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client: Sent connection request.\n");

    // Open the shared memory file
    int shm_fd = open(SEMAPHORE_FILE, O_RDWR, 0666); // Open existing shared memory file
    if (shm_fd == -1) {
        perror("Client: Failed to open shared memory file");
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    // Map the shared memory file into memory
    sem_t *sem_connect = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sem_connect == MAP_FAILED) {
        perror("Client: Failed to map shared memory");
        close(shm_fd);
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }
    close(shm_fd); // The file descriptor is no longer needed after mmap

    // Signal the server's semaphore
    if (sem_post(sem_connect) == -1) {
        perror("Client: Failed to signal semaphore");
        munmap(sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client: Signaled semaphore.\n");

    // Wait for acknowledgment from the server
    ssize_t bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("Client: Failed to receive acknowledgment from server");
        munmap(sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    if (strcmp(buffer, "REJECT") == 0) {
        printf("Client: Connection rejected by server (server full).\n");
        munmap(sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_SUCCESS); // Gracefully terminate
    } else if (strcmp(buffer, "ACCEPT") == 0) {
        printf("Client: Connection accepted by server.\n");
    } else {
        printf("Client: Unknown response from server: %s\n", buffer);
        munmap(sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    // Command loop for communication with the server
    while (1) {
        char command[BUFFER_SIZE];

        printf("Enter command (PLACE x y length orientation | ATTACK x y | QUIT): ");

        // Get user input
        fgets(command, sizeof(command), stdin);

        // Remove newline character from input
        command[strcspn(command, "\n")] = '\0';

        // Send command to the server
        if (send_message(write_fd, command) != 0) {
            perror("Client: Failed to send command");
            break;
        }

        // Exit loop if the client sends QUIT command
        if (strcmp(command, "QUIT") == 0) {
            printf("Client: Disconnecting from server.\n");
            break;
        }

        // Receive response from the server
        bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        if (bytes_received <= 0) {
            perror("Client: Failed to receive response from server");
            break;
        }

        buffer[bytes_received] = '\0'; // Null-terminate the string

        printf("Server response: %s\n", buffer);

        // Exit if the server indicates game over or disconnection
        if (strcmp(buffer, "GAME_OVER") == 0 || strcmp(buffer, "DISCONNECTED") == 0) {
            printf("Client: Server ended communication.\n");
            break;
        }
    }

    // Clean up resources before exiting
    munmap(sem_connect, sizeof(sem_t));
    close(write_fd);
    close(read_fd);

    printf("Client: Shutting down.\n");
}



int connect_to_server(const char *server_fifo, const char *client_fifo) {
    char connect_msg[BUFFER_SIZE];
    
    // Send client's unique FIFO path as connection message
    snprintf(connect_msg, sizeof(connect_msg), "%s", client_fifo);

    // Open server's write FIFO and send connection message
    int server_fd = pipe_open_write(server_fifo);
    if (server_fd == -1) {
        perror("Failed to open server FIFO for writing");
        return -1;
    }

    if (send_message(server_fd, connect_msg) != 0) {
        pipe_close(server_fd);
        return -1;
    }
    pipe_close(server_fd);

    printf("Connection request sent to server.\n");

    return 0;
}

void play_game(const char *server_fifo, const char *client_fifo) {
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];

    while (1) {
        printf("Enter command (PLACE x y length orientation | ATTACK x y | QUIT): ");

        // Get user input
        fgets(command, sizeof(command), stdin);

        // Remove newline character from input
        command[strcspn(command, "\n")] = '\0';

        // Open the server's write FIFO and send command
        int write_fd = pipe_open_write(server_fifo);
        if (write_fd == -1) {
            perror("Failed to open server FIFO for writing");
            break;
        }
        if (send_message(write_fd, command) != 0) {
            pipe_close(write_fd);
            break;
        }
        pipe_close(write_fd);

        // Exit loop if the client sends QUIT command
        if (strcmp(command, "QUIT") == 0) {
            break;
        }

        // Open the client's read FIFO and wait for response from server
        int read_fd = pipe_open_read(client_fifo);
        if (read_fd == -1) {
            perror("Failed to open client FIFO for reading");
            break;
        }
        if (receive_message(read_fd, buffer, sizeof(buffer)) != 0) {
            pipe_close(read_fd);
            break;
        }
        pipe_close(read_fd);

        printf("Server response: %s\n", buffer);

        if (strcmp(buffer, "GAME_OVER") == 0 || strcmp(buffer, "DISCONNECTED") == 0) {
            break;
        }
    }
}
