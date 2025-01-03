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
    snprintf(pipe_path, sizeof(pipe_path), "/tmp/server_%d", port);

    pipe_init(pipe_path);
    int fd = pipe_open_write(pipe_path);
    if (fd == -1) {
        pipe_destroy(pipe_path);
        return -1;
    }

    return fd;
}