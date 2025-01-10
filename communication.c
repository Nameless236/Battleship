#include "communication.h"
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int send_message(int fd, const char *message) {
    if (fd == -1) {
        fprintf(stderr, "Invalid file descriptor for writing.\n");
        return -1;
    }

    size_t len = strlen(message) + 1; // Include null terminator
    if (write(fd, message, len) == -1) {
        fprintf(stderr, "Failed to write to FIFO: %s\n", strerror(errno));
        return -1;
    }

    return 0;
}

int receive_message(int fd, char *buffer, size_t buffer_size) {
    if (fd == -1) {
        fprintf(stderr, "Invalid file descriptor for reading.\n");
        return -1;
    }

    ssize_t bytes_read = read(fd, buffer, buffer_size - 1); // Leave space for null terminator
    if (bytes_read > 0) {
        buffer[bytes_read] = '\0'; // Null-terminate the string
        printf("Message received: %s\n", buffer);
        return 0;
    } else if (bytes_read == 0) {
        fprintf(stderr, "No data available in FIFO.\n");
    } else {
        fprintf(stderr, "Failed to read from FIFO: %s\n", strerror(errno));
    }

    return -1;
}
