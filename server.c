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

#define SERVER_READ_FIFO "/tmp/server_read_fifo"
#define SERVER_WRITE_FIFO "/tmp/server_write_fifo"
#define SEMAPHORE_FILE "/home/pastorek10/sem_connect" // Path to shared memory file
#define SEMAPHORE_FILE_READ "/home/pastorek10/sem_read" // Path to shared memory file
#define SEMAPHORE_FILE_WRITE "/home/pastorek10/sem_write" // Path to shared memory file
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2 // Limit to two clients

pthread_mutex_t fifo_mutex; // Mutex for thread-safe access to FIFOs
sem_t *sem_connect;
sem_t * sem_read;
sem_t* sem_write;        // Pointer to unnamed semaphore in shared memory         
int connected_clients = 0;  // Track the number of connected clients

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

     // Declare an array to hold client information
    ClientInfo client_info[MAX_CLIENTS];

    // Declare an array for client threads
    pthread_t client_threads[MAX_CLIENTS];


    while (1) {
        char buffer[BUFFER_SIZE];

        printf("Server: Waiting for a connection...\n");

        // Wait for a client signal on the semaphore
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
            const char *accept_message = "ACCEPT";
            send_message(write_fd, accept_message);

            // Assign client ID and spawn a thread for handling this client
            client_info[connected_clients].client_id = connected_clients;
            pthread_create(&client_threads[connected_clients], NULL, handle_client, &client_info[connected_clients]);

            connected_clients++;
            printf("Server: Accepted client connection.\n");
        }

        close(read_fd);
        close(write_fd);
    }
    printf("Cleaning up;\n");
    cleanup_server();
}


void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg; // Cast argument to ClientInfo pointer
    char buffer[BUFFER_SIZE];

    printf("Handling client %d...\n", client_info->client_id);

    while (1) {
        pthread_mutex_lock(&fifo_mutex);
        printf("MUTEX\n");
        // Wait for a client message
        sem_wait(sem_read);
        printf("signal\n");

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

        // Process commands like PLACE, ATTACK, or QUIT
        if (strncmp(buffer, "PLACE", 5) == 0) {
            printf("Processing PLACE command from client %d...\n", client_info->client_id);
            // Add logic for placing ships on the game board
        } else if (strncmp(buffer, "ATTACK", 6) == 0) {
            printf("Processing ATTACK command from client %d...\n", client_info->client_id);
            // Add logic for attacking positions on the game board
        } else if (strcmp(buffer, "QUIT") == 0) {
            printf("Client %d requested to disconnect.\n", client_info->client_id);
            break;
        } else {
            printf("Unknown command from client %d: %s\n", client_info->client_id, buffer);
        }

        pthread_mutex_lock(&fifo_mutex);

        int write_fd = pipe_open_write(SERVER_WRITE_FIFO);
        if (write_fd == -1) {
            perror("Failed to open server write FIFO");
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        const char *response = "Command processed by server";
        if (send_message(write_fd, response) != 0) {
            perror("Failed to send response to client");
            pipe_close(write_fd);
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        pipe_close(write_fd);

        pthread_mutex_unlock(&fifo_mutex);

        // Signal that the client can write again
        sem_post(sem_write);
    }

    return NULL;
}

