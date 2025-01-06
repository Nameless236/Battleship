#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef CLIENT
#include "client.h"
void run_client();
#endif

#ifdef SERVER
#include "server.h"
#include <semaphore.h> // Use POSIX semaphore
extern sem_t fifo_semaphore; // Declare the semaphore
void run_server();
#endif

int main() {
#ifdef CLIENT
    run_client();
#endif

#ifdef SERVER
    run_server();

    // Destroy the semaphore after the server shuts down
    if (sem_destroy(&fifo_semaphore) != 0) {
        perror("Failed to destroy semaphore");
    }
#endif

    return 0;
}
