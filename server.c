#include "server.h"
#include "pipe.h"
#include "communication.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/mman.h> // For mmap
#include <sys/stat.h> // For mode constants
#include <sys/types.h>

#include "game-logic.h"

#define SERVER_READ_FIFO "/tmp/server_read_fifo"
#define SERVER_WRITE_FIFO "/tmp/server_write_fifo"
#define SEMAPHORE_FILE "/home/pastorek10/sem_connect" // Path to shared memory file
#define SEMAPHORE_FILE_READ "/home/pastorek10/sem_read" // Path to shared memory file
#define SEMAPHORE_FILE_WRITE "/home/pastorek10/sem_write" // Path to shared memory file
#define SHM_NAME "/battleship_shm"
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2 // Limit to two clients


pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t fifo_mutex; // Mutex for thread-safe access to FIFOs
sem_t *sem_connect;
sem_t * sem_read;
sem_t* sem_write;        // Pointer to unnamed semaphore in shared memory
int connected_clients = 0;  // Track the number of connected clients



GameData * shared_game_data = NULL;

void initialize_game(GameData *game_data) {
    pthread_mutex_lock(&game_mutex);

    initialize_board(&game_data->board_players[0]); // Herná mriežka pre hráča 1
    initialize_board(&game_data->board_players[1]); // Herná mriežka pre hráča 2
    game_data->player_turn = 0; // Hráč 1 začína
    game_data->client_id_1 = -1; // Nepripojený klient 1
    game_data->client_id_2 = -1; // Nepripojený klient 2

    pthread_mutex_unlock(&game_mutex);
}


sem_t *initialize_semaphore_file(const char *file) {
    // Create or open the shared memory file
    int shm_fd = open(file, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to create shared memory file");
        exit(EXIT_FAILURE);
    }

    // Resize the shared memory file to fit the semaphore
    if (ftruncate(shm_fd, sizeof(sem_t)) == -1) {
        perror("Failed to resize shared memory file");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }

    // Map the shared memory file into memory
    sem_t *semaphore = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (semaphore == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    close(shm_fd); // The file descriptor is no longer needed after mmap

    // Initialize the unnamed semaphore in shared memory
    if (sem_init(semaphore, 1, 0) == -1) { // Shared between processes, initial value = 0
        perror("Failed to initialize semaphore");
        munmap(semaphore, sizeof(sem_t));
        exit(EXIT_FAILURE);
    }

    return semaphore;
}


void initialize_server() {
    printf("Initializing FIFOs...\n");

    // Create FIFOs with proper permissions
    pipe_init(SERVER_READ_FIFO);
    pipe_init(SERVER_WRITE_FIFO);

    printf("Server initialized. Waiting for clients...\n");

    // Initialize named semaphores
    sem_unlink("/sem_connect");
    sem_unlink("/sem_read");
    sem_unlink("/sem_write");

    sem_connect = sem_open("/sem_connect", O_CREAT, 0666, 0); // Initial value = 0
    if (sem_connect == SEM_FAILED) {
        perror("Failed to create /sem_connect semaphore");
        exit(EXIT_FAILURE);
    }

    sem_read = sem_open("/sem_read", O_CREAT, 0666, 0); // Initial value = 0
    if (sem_read == SEM_FAILED) {
        perror("Failed to create /sem_read semaphore");
        exit(EXIT_FAILURE);
    }

    sem_write = sem_open("/sem_write", O_CREAT, 0666, 1); // Initial value = 1 (client can write)
    if (sem_write == SEM_FAILED) {
        perror("Failed to create /sem_write semaphore");
        exit(EXIT_FAILURE);
    }
    printf("Semaphores initialized.\n");
}


void cleanup_server() {
    // Destroy FIFOs
    pipe_destroy(SERVER_READ_FIFO);
    pipe_destroy(SERVER_WRITE_FIFO);

   // Close and unlink named semaphores
    if (sem_close(sem_connect) == -1) {
        perror("Failed to close /sem_connect semaphore");
    }
    if (sem_close(sem_read) == -1) {
        perror("Failed to close /sem_read semaphore");
    }
    if (sem_close(sem_write) == -1) {
        perror("Failed to close /sem_write semaphore");
    }

    sem_unlink("/sem_connect");
    sem_unlink("/sem_read");
    sem_unlink("/sem_write");

    pthread_mutex_destroy(&fifo_mutex);

    printf("Server shut down gracefully.\n");
}


void run_server() {
    initialize_server();

    // Initialize shared memory for GameData
    int shm_fd = shm_open("/battleship_shm", O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Failed to create shared memory");
        cleanup_server();
        exit(EXIT_FAILURE);
    }

    if (ftruncate(shm_fd, sizeof(GameData)) == -1) {
        perror("Failed to resize shared memory");
        close(shm_fd);
        cleanup_server();
        exit(EXIT_FAILURE);
    }

    GameData *shared_game_data = mmap(NULL, sizeof(GameData), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (shared_game_data == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        cleanup_server();
        exit(EXIT_FAILURE);
    }

    close(shm_fd);

    // Initialize game state in shared memory
    initialize_board(&shared_game_data->board_players[0]);
    initialize_board(&shared_game_data->board_players[1]);
    shared_game_data->player_turn = 0;
    shared_game_data->client_id_1 = -1;
    shared_game_data->client_id_2 = -1;

    printf("Shared memory initialized for game state.\n");

    ClientInfo client_info[MAX_CLIENTS];
    pthread_t client_threads[MAX_CLIENTS];
    int connected_clients = 0;

    while (1) {
        char buffer[BUFFER_SIZE];
        printf("Server: Waiting for a connection...\n");

        if (sem_wait(sem_connect) == -1) {
            perror("Server: Failed to wait on semaphore");
            continue;
        }

        printf("Server: Connection request received.\n");

        int read_fd = pipe_open_read(SERVER_READ_FIFO);
        if (read_fd == -1) {
            perror("Server: Failed to open read FIFO");
            continue;
        }

        int write_fd = pipe_open_write(SERVER_WRITE_FIFO);
        if (write_fd == -1) {
            perror("Server: Failed to open write FIFO");
            close(read_fd);
            continue;
        }

        ssize_t bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        if (bytes_received < 0) {
            perror("Server: Failed to receive connection message");
            close(read_fd);
            close(write_fd);
            continue;
        }

        if (connected_clients >= MAX_CLIENTS) {
            const char *reject_message = "REJECT";
            send_message(write_fd, reject_message);
            printf("Server: Rejected client connection (server full).\n");
        } else {
            printf("Server: Accepting connection from Client %d.\n", connected_clients);

            client_info[connected_clients].client_id = connected_clients;
            client_info[connected_clients].game_data = shared_game_data; // Use shared memory

            if (connected_clients == 0) {
                shared_game_data->client_id_1 = connected_clients;
            } else if (connected_clients == 1) {
                shared_game_data->client_id_2 = connected_clients;
            }

            // Send player index to the client
            char accept_message[BUFFER_SIZE];
            snprintf(accept_message, sizeof(accept_message), "ACCEPT %d", connected_clients); // Include player index
            send_message(write_fd, accept_message);

            pthread_create(&client_threads[connected_clients], NULL, handle_client, &client_info[connected_clients]);

            connected_clients++;
        }

        close(read_fd);
        close(write_fd);
    }

    cleanup_server();

    munmap(shared_game_data, sizeof(GameData));
    shm_unlink("/battleship_shm");

    printf("Server shut down gracefully.\n");
}


void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg;
    GameData *game_data = client_info->game_data;

    char buffer[BUFFER_SIZE];
    printf("Handling client %d...\n", client_info->client_id);

    int player_index = (client_info->client_id == game_data->client_id_1) ? 0 : 1;

    while (1) {
        sem_wait(sem_read); // Wait for a client message

        pthread_mutex_lock(&fifo_mutex);
        int read_fd = pipe_open_read(SERVER_READ_FIFO);
        if (read_fd == -1) {
            perror("Failed to open server read FIFO");
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        ssize_t bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        pipe_close(read_fd);
        pthread_mutex_unlock(&fifo_mutex);

        if (bytes_received < 0) {
            printf("Client %d disconnected or error occurred.\n", client_info->client_id);
            break;
        }

        printf("Message from client %d: %s\n", client_info->client_id, buffer);

        if (strncmp(buffer, "PLACE", 5) == 0) {
            printf("Processing PLACE command from client %d...\n", client_info->client_id);

            int x, y, length;
            char orientation;

            sscanf(buffer, "PLACE %d %d %d %c", &x, &y, &length, &orientation);

            printf("x %d y %d length %d orientation %c\n", x, y, length, orientation);

            int result = place_ship(&game_data->board_players[player_index], x, y, length, orientation);

            const char *response;
            if (result == 1) {
                response = "PLACE_RESPONSE 1";
                printf("Ship placement successful for Client %d.\n", client_info->client_id);
            } else {
                response = "PLACE_RESPONSE 0";
                printf("Ship placement failed for Client %d.\n", client_info->client_id);
            }

            pthread_mutex_lock(&fifo_mutex);
            int write_fd = pipe_open_write(SERVER_WRITE_FIFO);
            if (write_fd != -1) {
                send_message(write_fd, response);
                pipe_close(write_fd);
            }
            pthread_mutex_unlock(&fifo_mutex);

            // Check if all ships are placed for both players
            if (game_data->board_players[0].ships_remaining >= 5 && game_data->board_players[0].ships_remaining >= 5) {
                printf("Both players have placed all their ships. Starting the game...\n");
                break; // Exit ship placement phase
            }
        } else if (strcmp(buffer, "QUIT") == 0) {
            printf("Client %d requested to disconnect.\n", client_info->client_id);
            break;
        } else {
            printf("Unknown command from client %d: %s\n", client_info->client_id, buffer);
        }

        sem_post(sem_write); // Signal that the client can write again
    }

    return NULL;
}
