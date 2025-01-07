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
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2 // Limit to two clients

pthread_mutex_t fifo_mutex; // Mutex for thread-safe access to FIFOs
sem_t *sem_connect;         // Pointer to unnamed semaphore in shared memory
sem_t sem_read, sem_write;         
int connected_clients = 0;  // Track the number of connected clients

void initialize_server() {
    printf("Initializing FIFOs...\n");

    // Create FIFOs with proper permissions
    pipe_init(SERVER_READ_FIFO);
    pipe_init(SERVER_WRITE_FIFO);

    printf("Server initialized. Waiting for clients...\n");

    // Create or open the shared memory file
    int shm_fd = open(SEMAPHORE_FILE, O_CREAT | O_RDWR, 0666);
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
    sem_connect = mmap(NULL, sizeof(sem_t), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    if (sem_connect == MAP_FAILED) {
        perror("Failed to map shared memory");
        close(shm_fd);
        exit(EXIT_FAILURE);
    }
    close(shm_fd); // The file descriptor is no longer needed after mmap

    // Initialize the unnamed semaphore in shared memory
    if (sem_init(sem_connect, 1, 0) == -1) { // Shared between processes, initial value = 0
        perror("Failed to initialize semaphore");
        munmap(sem_connect, sizeof(sem_t));
        exit(EXIT_FAILURE);
    }
}

void cleanup_server() {
    // Destroy FIFOs
    pipe_destroy(SERVER_READ_FIFO);
    pipe_destroy(SERVER_WRITE_FIFO);

    // Destroy the unnamed semaphore
    if (sem_destroy(sem_connect) == -1) {
        perror("Failed to destroy semaphore");
    }

    // Unmap and remove the shared memory file
    if (munmap(sem_connect, sizeof(sem_t)) == -1) {
        perror("Failed to unmap shared memory");
    }
    if (unlink(SEMAPHORE_FILE) == -1) {
        perror("Failed to unlink shared memory file");
    }

    pthread_mutex_destroy(&fifo_mutex);
    printf("Server shut down gracefully.\n");
}

void run_server() {
    initialize_server();

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
            connected_clients++;
            printf("Server: Accepted client connection.\n");
        }

        close(read_fd);
        close(write_fd);
    }

    cleanup_server();
}


void *handle_client(void *arg) {
    ClientInfo *client_info = (ClientInfo *)arg; // Cast argument to ClientInfo pointer
    char buffer[BUFFER_SIZE];

    printf("Handling client %d...\n", client_info->client_id);

    while (1) {
        pthread_mutex_lock(&fifo_mutex);

        // Wait for a client message
        sem_wait(&sem_read);

        int read_fd = pipe_open_read(SERVER_READ_FIFO);
        if (read_fd == -1) {
            perror("Failed to open server read FIFO");
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        ssize_t bytes_received = receive_message(read_fd, buffer, BUFFER_SIZE);
        pipe_close(read_fd);

        pthread_mutex_unlock(&fifo_mutex);

        if (bytes_received <= 0) {
            printf("Client %d disconnected or error occurred.\n", client_info->client_id);
            break;
        }

        printf("Message from client %d: %s\n", client_info->client_id, buffer);

        pthread_mutex_lock(&fifo_mutex);

        int write_fd = pipe_open_write(SERVER_WRITE_FIFO);
        if (write_fd == -1) {
            perror("Failed to open server write FIFO");
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        const char *response = "Message received by server";
        if (send_message(write_fd, response) != 0) {
            perror("Failed to send response to client");
            pipe_close(write_fd);
            pthread_mutex_unlock(&fifo_mutex);
            break;
        }

        pipe_close(write_fd);

        pthread_mutex_unlock(&fifo_mutex);

        // Signal that the client can write again
        sem_post(&sem_write);

        // Exit if the client sends "QUIT"
        if (strcmp(buffer, "QUIT") == 0) {
            printf("Client %d requested to disconnect.\n", client_info->client_id);
            break;
        }
    }

    return NULL;
}
