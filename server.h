#pragma once

// Inicializuje server na danom porte
void initialize_server(int port);

// Čaká na pripojenie klienta a vracia socket
int accept_connection(int server_fd);

// Spracováva komunikáciu s klientom
void handle_client(int client_socket);

// Posiela správu všetkým klientom okrem jedného
void broadcast_message(const char *message, int exclude_client);

