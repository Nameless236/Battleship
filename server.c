#include "server.h"
#include "pipe.h"
#include "communication.h"
#include "game-logic.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <semaphore.h>
#include <fcntl.h>
#include <sys/stat.h>

static pthread_mutex_t game_mutex = PTHREAD_MUTEX_INITIALIZER;
static GameData game_data = {0};

void initialize_semaphore(const char *sem_name, sem_t **sem, int initial_value) {
    sem_unlink(sem_name);
    *sem = sem_open(sem_name, O_CREAT | O_EXCL, 0666, initial_value);
    if (*sem == SEM_FAILED) {
        perror("Failed to create semaphore");
        exit(EXIT_FAILURE);
    }
    sem_close(*sem);

}

void initialize_fifo(const char *fifo_name) {
    unlink(fifo_name);
    pipe_init(fifo_name);
}

void initialize_server(const char *server_name) {
    printf("Initializing server: %s...\n", server_name);

    // Generate unique semaphore names
    char sem_connect_name[BUFFER_SIZE], sem_command_name[BUFFER_SIZE];
    char sem_response_client0_name[BUFFER_SIZE], sem_response_client1_name[BUFFER_SIZE];
    char sem_continue1_name[BUFFER_SIZE], sem_continue2_name[BUFFER_SIZE];

    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);
    snprintf(sem_response_client0_name, sizeof(sem_response_client0_name), SEM_RESPONSE_TEMPLATE, server_name, 0);
    snprintf(sem_response_client1_name, sizeof(sem_response_client1_name), SEM_RESPONSE_TEMPLATE, server_name, 1);

    snprintf(sem_continue1_name, sizeof(sem_continue1_name), SEM_CONTINUE_TEMPLATE, server_name, 0);
    snprintf(sem_continue2_name, sizeof(sem_continue2_name), SEM_CONTINUE_TEMPLATE, server_name, 1);

    mode_t old_umask = umask(0);
    // Initialize semaphores
    sem_t *sem_connect;
    sem_connect = sem_open(sem_connect_name, O_EXCL);
    if (sem_connect == SEM_FAILED) {
        perror("Failed to create semaphore");
        exit(EXIT_FAILURE);
    }

    sem_t *sem_command;
    initialize_semaphore(sem_command_name, &sem_command, 0);

    sem_t *sem_continue1;
    initialize_semaphore(sem_continue1_name, &sem_continue1, 0);

    sem_t *sem_continue2;
    initialize_semaphore(sem_continue2_name, &sem_continue2, 0);

    sem_t *sem_response_client0;
    initialize_semaphore(sem_response_client0_name, &sem_response_client0, 0);

    sem_t *sem_response_client1;
    initialize_semaphore(sem_response_client1_name, &sem_response_client1, 0);
    umask(old_umask);

    // Initialize FIFOs
    char server_read_fifo[BUFFER_SIZE], server_write_fifo[BUFFER_SIZE], client_write_fifo1[BUFFER_SIZE],  client_write_fifo2[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    snprintf(client_write_fifo1, sizeof(client_write_fifo1), CLIENT_READ_FIFO_TEMPLATE, server_name, 0);
    snprintf(client_write_fifo2, sizeof(client_write_fifo2), CLIENT_READ_FIFO_TEMPLATE, server_name, 1);

    initialize_fifo(server_read_fifo);
    initialize_fifo(server_write_fifo);
    initialize_fifo(client_write_fifo1);
    initialize_fifo(client_write_fifo2);

    printf("Server initialized and ready.\n");

    // Signal readiness
    sem_post(sem_connect);

    // Close unused semaphore handles
    sem_close(sem_connect);
}

void cleanup_server(const char *server_name) {
    char client_write_fifo[BUFFER_SIZE];
    snprintf(client_write_fifo, sizeof(client_write_fifo), CLIENT_READ_FIFO_TEMPLATE, server_name, 0);
    unlink(client_write_fifo);

    snprintf(client_write_fifo, sizeof(client_write_fifo), CLIENT_READ_FIFO_TEMPLATE, server_name, 1);
    unlink(client_write_fifo);

    // Generate FIFO paths
    char server_read_fifo[BUFFER_SIZE];
    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    // Unlink FIFOs
    unlink(server_read_fifo);
    unlink(server_write_fifo);

    // Unlink semaphores
    char sem_connect_name[BUFFER_SIZE], sem_command_name[BUFFER_SIZE];
    char sem_response_client0_name[BUFFER_SIZE], sem_response_client1_name[BUFFER_SIZE];
    char sem_continue1_name[BUFFER_SIZE], sem_continue2_name[BUFFER_SIZE];

    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);
    snprintf(sem_response_client0_name, sizeof(sem_response_client0_name), SEM_RESPONSE_TEMPLATE, server_name, 0);
    snprintf(sem_response_client1_name, sizeof(sem_response_client1_name), SEM_RESPONSE_TEMPLATE, server_name, 1);
    snprintf(sem_continue1_name, sizeof(sem_continue1_name), SEM_CONTINUE_TEMPLATE, server_name, 0);
    snprintf(sem_continue2_name, sizeof(sem_continue2_name), SEM_CONTINUE_TEMPLATE, server_name, 1);

    sem_unlink(sem_connect_name);
    sem_unlink(sem_command_name);
    sem_unlink(sem_response_client0_name);
    sem_unlink(sem_response_client1_name);

    sem_unlink(sem_continue1_name);
    sem_unlink(sem_continue2_name);
    
    // Destroy mutex
    pthread_mutex_destroy(&game_mutex);
}

void send_message_to_client(int client_id, const char *server_name, const char *message) {
    char client_write_fifo[BUFFER_SIZE];
    snprintf(client_write_fifo, sizeof(client_write_fifo), CLIENT_READ_FIFO_TEMPLATE, server_name, client_id);

    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    char sem_response_name[BUFFER_SIZE];
    snprintf(sem_response_name, sizeof(sem_response_name), SEM_RESPONSE_TEMPLATE, server_name, client_id);

    sem_t *sem_response = sem_open(sem_response_name, O_RDWR);

    int write_fd_s = pipe_open_write(server_write_fifo);
    int write_fd_c = pipe_open_write(client_write_fifo);
    if (write_fd_s != -1 && strncmp(message, "CLIENT_ID:", 10) == 0) {
        send_message(write_fd_s, message);
    }
    else if (write_fd_c != -1) {
            // For all other messages, add the client prefix
            char prefixed_message[BUFFER_SIZE];
            snprintf(prefixed_message, sizeof(prefixed_message), "CLIENT_%d:%s", client_id, message);

            send_message(write_fd_c, prefixed_message);
            sem_post(sem_response);
    }
    pipe_close(write_fd_s);
    pipe_close(write_fd_c);

    sem_close(sem_response);
}

void handle_client_message(int client_id, const char *message, const char *server_name, GameData *game_data) {
    char sem_continue1_name[BUFFER_SIZE], sem_continue2_name[BUFFER_SIZE];
    snprintf(sem_continue1_name, sizeof(sem_continue1_name), SEM_CONTINUE_TEMPLATE, server_name, 0);
    snprintf(sem_continue2_name, sizeof(sem_continue2_name), SEM_CONTINUE_TEMPLATE, server_name, 1);

    sem_t * sem_continue1 = sem_open(sem_continue1_name, O_RDWR);
    sem_t *sem_continue2 = sem_open(sem_continue2_name, O_RDWR);

    if (strncmp(message, "SEND_BOARD", 10) == 0) {
        GameBoard *board = &game_data->board_players[client_id];

        // Decode the board: Replace 'A' with 0 and 'B' with 1
        int index = 11;
        for (int i = 0; i < BOARD_SIZE; i++) {
            for (int j = 0; j < BOARD_SIZE; j++) {
                board->grid[i][j] = message[index++] - 'A';
            }
        }

        // Acknowledge receipt of the board
        char response[BUFFER_SIZE];
        snprintf(response, sizeof(response), "BOARD_RECEIVED");
        send_message_to_client(client_id, server_name, response);
    } else if (strncmp(message, "ATTACK", 6) == 0) {
        int x, y;
        if (sscanf(message + 7, "%d_%d", &x, &y) == 2 && client_id == game_data->player_turn) {
            int opponent_id = (client_id == 0) ? 1 : 0;
            GameBoard *opponent_board = &game_data->board_players[opponent_id];

            int result = attack(opponent_board, x, y);

            // Notify attacking client of result
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "ATTACK_RESULT_%c_%d_%d", (result == 1 || result == 2) ? 'H' : 'M', x, y);
            send_message_to_client(client_id, server_name, response);
            sem_wait(client_id == 0 ? sem_continue1 : sem_continue2);

            // Notify opponent of attack
            snprintf(response, sizeof(response), "OPPONENT_ATTACKED_%c_%d_%d", (result == 1 || result == 2 ) ? 'H' : 'M', x, y);
            send_message_to_client(opponent_id, server_name, response);
            sem_wait(client_id == 0 ? sem_continue2 : sem_continue1);

            // Check for game over condition
            if (result == 2) { // All ships sunk
                send_message_to_client(client_id, server_name, "GAME_OVER_W"); // Attacking player wins
                sem_wait(client_id == 0 ? sem_continue1 : sem_continue2);
                send_message_to_client(opponent_id, server_name, "GAME_OVER_L"); // Opponent loses
                sem_wait(client_id == 0 ? sem_continue2 : sem_continue1);

                sem_close(sem_continue1);
                sem_close(sem_continue2);
                cleanup_server(server_name);
                exit(EXIT_SUCCESS);
            }

            // Switch turns
            game_data->player_turn = opponent_id;
        } else {
            send_message_to_client(client_id, server_name, "WRONG_TURN");
        }
    } else if (strncmp(message, "QUIT", 4) == 0) {
        int opponent_id = (client_id == 0) ? 1 : 0;

        send_message_to_client(opponent_id, server_name, "OPPONENT_QUIT");
        send_message_to_client(client_id, server_name, "MY_QUIT");
        sem_close(sem_continue1);
        sem_close(sem_continue2);

        cleanup_server(server_name);
        exit(EXIT_SUCCESS);
    }
    sem_close(sem_continue1);
    sem_close(sem_continue2);
}

void run_server(const char *server_name) {
    initialize_server(server_name);

    char sem_command_name[BUFFER_SIZE];
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);

    sem_t *sem_command = sem_open(sem_command_name, O_CREAT | O_RDWR);
    
    if (sem_command == SEM_FAILED) {
        perror("Failed to open command semaphore");
        cleanup_server(server_name);
        exit(EXIT_FAILURE);
    }

    char server_read_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);

    int connected_clients = 0;

    int read_fd = pipe_open_read(server_read_fifo);

    while (1) {
        sem_wait(sem_command); // Wait for a command from a client

        char buffer[BUFFER_SIZE];
        if (receive_message(read_fd, buffer, BUFFER_SIZE) == 0) {
            int client_id;
            char message[BUFFER_SIZE];
            if (strncmp(buffer, "CONNECT", 7) == 0) {
                pthread_mutex_lock(&game_mutex);

                if (connected_clients < MAX_CLIENTS) {
                    int new_client_id = connected_clients;
                    char response[BUFFER_SIZE];

                    // Assign a new client ID and send it to the client
                    snprintf(response, sizeof(response), "CLIENT_ID:%d", new_client_id);
                    send_message_to_client(new_client_id, server_name, response);

                    // Update game data for connected clients
                    if (new_client_id == 0) {
                        game_data.client_id_1 = new_client_id;
                    } else if (new_client_id == 1) {
                        game_data.client_id_2 = new_client_id;
                    }

                    connected_clients++;
                } else {
                    char server_write_fifo[BUFFER_SIZE];
                    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

                    int write_fd = pipe_open_write(server_write_fifo);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "REJECT");
                    sem_close(sem_command);
                    pipe_close(write_fd);
                    pipe_close(read_fd);
                    send_message(write_fd, response);
                }

                pthread_mutex_unlock(&game_mutex);
            } else if (sscanf(buffer, "CLIENT_%d:%s", &client_id, message) == 2) {
                pthread_mutex_lock(&game_mutex);
                if (strstr(message, "QUIT") != NULL) {
                    pipe_close(read_fd);
                    sem_close(sem_command);
                }
                handle_client_message(client_id, message, server_name, &game_data);
                pthread_mutex_unlock(&game_mutex);
            }  
        }
    }

    pipe_close(read_fd);
    sem_close(sem_command);
}
