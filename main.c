#include <stdio.h>
#include <stdlib.h>

#ifdef CLIENT
#include "client.h"
#endif

#ifdef SERVER
#include "server.h"
#endif

int main(int argc, char *argv[]) {
    #ifdef CLIENT
    run_client(argc, argv);
    #endif

    #ifdef SERVER
    run_server(argv[1]);
    #endif

    return 0;
}
