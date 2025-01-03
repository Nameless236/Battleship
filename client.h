#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

// Pripojí klienta na server na zadaný port
int connect_to_server(const char *server_ip, int port);

// Posiela správu na server
void send_message(const char *path, const char *message);

// Prijíma správu od servera
void receive_message(const char *client_fifo, char *buffer, size_t buffer_size);

