#ifdef CLIENT
#include "client.h"
#endif

#ifdef SERVER
#include "server.h"
#endif

int main() {
#ifdef CLIENT
    run_client();
#endif

#ifdef SERVER
    run_server();
#endif

    return 0;
}
