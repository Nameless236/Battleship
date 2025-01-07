#pragma once

#include <stddef.h>

// Send a message through a file descriptor
int send_message(int fd, const char *message);

// Receive a message from a file descriptor
int receive_message(int fd, char *buffer, size_t buffer_size);
