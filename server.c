#include "server.h"

#define SERVER_FIFO_PATH "server_fifo"
#define CLIENT_FIFO_TEMPLATE "client_fifo_%d"

void initialize_server(void) {
  if (sem_init(&fifo_semaphore, 0, 1) != 0) {
    perror("Failed to initialize semaphore");
    exit(1);
  }
  // Vytvoríme FIFO s názvom server_fifo
  if (mkfifo(SERVER_FIFO_PATH, 0666) == -1) {
      perror("Failed to create server FIFO");
      exit(1);
  }

  printf("Server FIFO created at path: %s\n", SERVER_FIFO_PATH);
}


void accept_connection(void) {
  char client_fifo[256];
  char buffer[1024];
  int client_id = 0;

  while (1) {
    sem_wait(&fifo_semaphore);
    int fd = open(SERVER_FIFO_PATH, O_RDONLY);
    sem_post(&fifo_semaphore);

    if (fd == -1) {
      perror("Failed to open server FIFO for reading");
      continue;
    }

    // Čítanie mena klientského FIFO
    if (read(fd, buffer, sizeof(buffer)) > 0) {
      snprintf(client_fifo, sizeof(client_fifo), CLIENT_FIFO_TEMPLATE, client_id++);
      printf("Client connected: %s\n", buffer);

      // Vytvorenie vlákna na obsluhu klienta
      pthread_t client_thread;
      char *fifo_name = strdup(client_fifo); // Prenos mena FIFO do vlákna

      if (pthread_create(&client_thread, NULL, handle_client, fifo_name) != 0) {
        perror("Failed to create thread");
      }

      pthread_detach(client_thread); // Automatické čistenie vlákna po ukončení
    }
    close(fd);
    }
}