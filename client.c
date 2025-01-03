#include "client.h"

#define CLIENT_FIFO_PATH "client_fifo"

void send_message(const char *client_fifo, const char *message) {
  int fd = open(client_fifo, O_WRONLY);
  if (fd == -1) {
    perror("Failed to open FIFO for writing");
    exit(1);
  }

  // Odoslanie nazvu FIFO a spravu
  char full_message[1024];
  snprintf(full_message, sizeof(full_message), "%s:%s", client_fifo, message);
  write(fd, full_message, strlen(full_message) + 1);

  printf("Message sent to server: %s\n", message);
  close(fd);
}

void receive_message(const char *client_fifo, char *buffer, size_t buffer_size) {
  // Otvorenie FIFO pre čítanie
  int fd = open(client_fifo, O_RDONLY);
  if (fd == -1) {
    perror("Failed to open FIFO for reading");
    exit(1);
  }

  // Čakanie na správu od servera
  memset(buffer, 0, buffer_size); // Vycistenie buffera
  if (read(fd, buffer, buffer_size) > 0) {
    printf("Message received from server: %s\n", buffer);
  } else {
    perror("Failed to read from client FIFO");
  }

  close(fd);
}