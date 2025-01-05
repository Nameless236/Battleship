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
#include <dispatch/dispatch.h>

extern dispatch_semaphore_t fifo_semaphore; // Deklarácia natívneho semaforu

typedef struct {
    char fifo_path[256];
    int client_id;
} ClientData;

// Inicializuje server na danom porte
void initialize_server(void);

// Čaká na pripojenie klienta a spustí vlákno na jeho obsluhu
void accept_connection(void);

// Spracováva komunikáciu s klientom
void handle_client(void *arg);

// Posiela správu všetkým klientom okrem jedného
void broadcast_message(const char *message, int exclude_client);

// Prijima správu na daný file descriptor
void receive_message(const char *path, char *buffer, size_t buffer_size);

// Posiela správu klientovi
void send_message(const char *path, const char *message);

