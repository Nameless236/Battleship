#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#ifdef CLIENT
#include "client.h"
void run_client();
#endif

#ifdef SERVER
#include "server.h"
#include <dispatch/dispatch.h>
extern dispatch_semaphore_t fifo_semaphore; // Deklarácia semaforu
void run_server();
#endif

int main() {
#ifdef CLIENT
    run_client();
#endif

#ifdef SERVER
    run_server();

    // Uvoľnenie semaforu po ukončení servera
    if (fifo_semaphore != NULL) {
        dispatch_release(fifo_semaphore);
    }
#endif

    return 0;
}
