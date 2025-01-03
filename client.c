#include "klient.h"
#include "pipe.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <stdlib.h>


int connect_to_server(const char *server_ip, int port) {
    char pipe_path[256];
    char client_pipe_path[256];
    int client_id = getpid(); // Use process ID as unique client identifier

    // Create paths for bidirectional communication
    snprintf(pipe_path, sizeof(pipe_path), "/tmp/server_%d", port);
    snprintf(client_pipe_path, sizeof(client_pipe_path), "/tmp/client_%d", client_id);

    // Create client's pipe for receiving messages
    pipe_init(client_pipe_path);

    // Open server pipe for writing
    int server_fd = pipe_open_write(pipe_path);
    if (server_fd == -1) {
        pipe_destroy(client_pipe_path);
        return -1;
    }

    // Send connection request with client ID
    char connect_msg[256];
    snprintf(connect_msg, sizeof(connect_msg), "CONNECT %d", client_id);
    send_message(server_fd, connect_msg);

    // Open client pipe for reading server responses
    int client_fd = pipe_open_read(client_pipe_path);
    if (client_fd == -1) {
        pipe_close(server_fd);
        pipe_destroy(client_pipe_path);
        return -1;
    }

    // Wait for server acknowledgment
    char buffer[1024];
    receive_message(client_fd, buffer, sizeof(buffer));

    if (strncmp(buffer, "ACCEPT", 6) != 0) {
        pipe_close(server_fd);
        pipe_close(client_fd);
        pipe_destroy(client_pipe_path);
        return -1;
    }

    return server_fd;
}
