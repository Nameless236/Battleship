#pragma once

#include <pthread.h>

// Mutex for synchronizing access to FIFOs
extern pthread_mutex_t fifo_mutex;

// Structure to store client-specific information
typedef struct {
    int client_id; // Unique ID for the client
} ClientInfo;

// Initialize the server resources
void initialize_server(void);

// Run the server loop to handle clients
void run_server(void);

// Handle communication with a single client (thread function)
void *handle_client(void *arg);
