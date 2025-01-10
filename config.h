#ifndef CONFIG_H
#define CONFIG_H

#define BUFFER_SIZE 1024
#define MAX_CLIENTS 2
#define BOARD_SIZE 10

// Semaphore templates
#define SEM_CONNECT_TEMPLATE "/sem_connect_%s"
#define SEM_COMMAND_TEMPLATE "/sem_command_%s"
#define SEM_RESPONSE_TEMPLATE "/sem_response_%s_%d"
#define SEM_BOARD_TEMPLATE "/sem_board_%s"

// FIFO templates
#define SERVER_READ_FIFO_TEMPLATE "/tmp/%s_server_read"
#define SERVER_WRITE_FIFO_TEMPLATE "/tmp/%s_server_write"

#endif
