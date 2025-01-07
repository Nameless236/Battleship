#include "client.h"
#include "communication.h"
#include "game-logic.h"
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
#define SEMAPHORE_FILE_READ "/home/pastorek10/sem_read" // Path to shared memory file
#define SEMAPHORE_FILE_WRITE "/home/pastorek10/sem_write" // Path to shared memory file
#define BUFFER_SIZE 1024

sem_t *open_semaphore(const char *file) {
    // Open the shared memory file
    int shm_fd = open(file, O_RDWR, 0666); // Open existing shared memory file
    if (shm_fd == -1) {
        perror("Failed to open shared memory file");
        return NULL;
    }

    // Map the shared memory file into memory
    sem_t *semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (semaphore == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        return NULL;
    }

    close(shm_fd); // File descriptor no longer needed after mmap
    return semaphore;
}

void run_client() {
    GameBoard board;
    initialize_board(&board);

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

    // Open existing named semaphores
    sem_t *sem_connect = sem_open("/sem_connect", 0); // Open without creating
    if (sem_connect == SEM_FAILED) {
        perror("Client: Failed to open /sem_connect semaphore");
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_read = sem_open("/sem_read", 0); // Open without creating
    if (sem_read == SEM_FAILED) {
        perror("Client: Failed to open /sem_read semaphore");
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    sem_t *sem_write = sem_open("/sem_write", 0); // Open without creating
    if (sem_write == SEM_FAILED) {
        perror("Client: Failed to open /sem_write semaphore");
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    printf("%p\n", &sem_connect);

    // Signal the server's semaphore
    if (sem_post(sem_connect) == -1) {
        perror("Client: Failed to signal semaphore");
        munmap(&sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }
    printf("Client: Signaled semaphore.\n");

    // Wait for acknowledgment from the server
    ssize_t bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
    if (bytes_received < 0) {
        perror("Client: Failed to receive acknowledgment from server");
        munmap(&sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    if (strcmp(buffer, "REJECT") == 0) {
        printf("Client: Connection rejected by server (server full).\n");
        munmap(&sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_SUCCESS); // Gracefully terminate
    } else if (strcmp(buffer, "ACCEPT") == 0) {
        printf("Client: Connection accepted by server.\n");
    } else {
        printf("Client: Unknown response from server: %s\n", buffer);
        munmap(&sem_connect, sizeof(sem_t));
        close(write_fd);
        close(read_fd);
        exit(EXIT_FAILURE);
    }

    int ships_to_place = 5;

    while (ships_to_place > 0) {
        printf("Place your ships. Remaining: %d\n", ships_to_place);
        printf("Enter command (PLACE x y length orientation): ");

        // Get user input
        fgets(buffer, sizeof(buffer), stdin);

        // Remove newline character from input
        buffer[strcspn(buffer, "\n")] = '\0';

        // Ensure only PLACE commands are allowed
        if (strncmp(buffer, "PLACE", 5) != 0) {
            printf("Invalid command. You must place all ships first.\n");
            continue;
        }

        // Send PLACE command to the server
        if (send_message(write_fd, buffer) != 0) {
            perror("Client: Failed to send command");
            break;
        }

        sem_post(sem_read);

        // Wait for server response
        bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            perror("Client: Failed to receive response from server");
            break;
        }

        printf("Server response: %s\n", buffer);

        if (strcmp(buffer, "PLACE_RESPONSE 1") == 0) {
            ships_to_place--; // Successfully placed a ship
            // Receive updated board from server
            bytes_received = receive_message(read_fd, board.grid, sizeof(board.grid));
            if (bytes_received < 0) {
                perror("Client: Failed to receive updated board from server");
                break;
            }
            printf("Board updated from server:\n");
            print_board(&board);
        } else {
            printf("Failed to place ship. Try again.\n");
        }
    }

    // Command loop for communication with the server
    while (1) {
        char command[BUFFER_SIZE];

        while (board.ships_remaining >= 0) {
            print_board(&board);
        }

        printf("Enter command (ATTACK x y | QUIT): ");

        // Get user input
        fgets(command, sizeof(command), stdin);

        // Remove newline character from input
        command[strcspn(command, "\n")] = '\0';

        // Send command to the server
        if (send_message(write_fd, command) != 0) {
            perror("Client: Failed to send command");
            break;
        }

        sem_post(sem_read);

        // Exit loop if the client sends QUIT command
        if (strcmp(command, "QUIT") == 0) {
            printf("Client: Disconnecting from server.\n");
            break;
        }

        // Wait for response from the server
        bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
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

    munmap(sem_connect, sizeof(sem_t));
     munmap(sem_read, sizeof(sem_t));
     munmap(sem_write, sizeof(sem_t));
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