#include "pipe.h"
#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>    // For O_CREAT and O_EXCL
#include <semaphore.h>
#include <sys/stat.h> // For mode constants
#include <errno.h>


void pipe_init(const char *path) {
  if (access(path, F_OK) == 0) { // Kontrola existencie FIFO
    if (unlink(path) == -1) {  // Odstránenie starého FIFO
      perror("Failed to unlink existing FIFO");
      exit(EXIT_FAILURE);
    }
  }


 // Temporarily set umask to 0
    mode_t old_umask = umask(0);

    // Create the FIFO with full read/write permissions for everyone
    if (mkfifo(path,  0666) == -1) {
        perror("Failed to create named pipe");
        exit(EXIT_FAILURE);
    }

    // Restore the original umask
    umask(old_umask);


}


void pipe_destroy(const char *path) {
  if (unlink(path) == -1) {
    perror("Failed to unlink named pipe");
    exit(EXIT_FAILURE);
  }
}

static int open_pipe(const char *path, int flags) {
  const int fd = open(path, flags);
  if (fd == -1) {
    fprintf(stderr, "Failed to open named pipe %s", path);
    perror("");
    exit(EXIT_FAILURE);
  }
  return fd;
}

int pipe_open_read(const char *path) {
    int fd;

    // Open the FIFO in read-write mode to avoid blocking
    fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open FIFO for reading");
        return -1;
    }

    return fd;
}

int pipe_open_write(const char *path) {
    int fd;

    // Open the FIFO in read-write mode to avoid blocking
    fd = open(path, O_RDWR);
    if (fd == -1) {
        perror("Failed to open FIFO for writing");
        return -1;
    }

    return fd;
}


void pipe_close(const int fd) {
  if (close(fd) == -1) {
    perror("Failed to close pipe");
    exit(EXIT_FAILURE);
  }
}


