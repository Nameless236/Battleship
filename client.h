#pragma once
#include <stdio.h>

// Pripojí klienta na server na zadaný port
int connect_to_server(const char *server_ip, int port);

// Posiela správu na server
void send_message(int socket, const char *message);

// Prijíma správu od servera
void receive_message(int socket, char *buffer, size_t buffer_size);
