#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>

// Declare a POSIX semaphore
extern sem_t fifo_semaphore;

typedef struct {
    char fifo_path[256];
    int client_id;
} ClientData;

// Initializes the server
void initialize_server(void);

// Waits for a client connection and starts a thread to handle it
void accept_connection(void);

// Handles communication with a client
void handle_client(void *arg);

// Sends a message to all clients except one
void broadcast_message(const char *message, int exclude_client);

// Receives a message from the given file descriptor
void receive_message(const char *path, char *buffer, size_t buffer_size);

// Sends a message to a client
void send_message(const char *path, const char *message);
