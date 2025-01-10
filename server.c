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

void initialize_game(GameData *game_data) {
    pthread_mutex_lock(&game_mutex);
    initialize_board(&game_data->board_players[0]);
    initialize_board(&game_data->board_players[1]);
    game_data->client_id_1 = -1;
    game_data->client_id_2 = -1;
    pthread_mutex_unlock(&game_mutex);
}


#include "server.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

void initialize_server(const char *server_name) {
    printf("Initializing server: %s...\n", server_name);

    // Generate unique semaphore names
    char sem_connect_name[BUFFER_SIZE];
    char sem_command_name[BUFFER_SIZE];
    char sem_response_client0_name[BUFFER_SIZE];
    char sem_response_client1_name[BUFFER_SIZE];
    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);
    snprintf(sem_response_client0_name, sizeof(sem_response_client0_name), SEM_RESPONSE_TEMPLATE, server_name, 0);
    snprintf(sem_response_client1_name, sizeof(sem_response_client1_name), SEM_BOARD_TEMPLATE, server_name, 1);

    // Unlink any existing semaphores with the same name

    sem_unlink(sem_command_name);
    sem_unlink(sem_response_client0_name);
    sem_unlink(sem_response_client1_name);

    // Create and initialize semaphores
    sem_t *sem_connect = sem_open(sem_connect_name, O_CREAT, 0666, 0);
    sem_t *sem_command = sem_open(sem_command_name, O_CREAT | O_EXCL, 0666, 0);
    sem_t *sem_response_client0 = sem_open(sem_response_client0_name, O_CREAT | O_EXCL, 0666, 0);
    sem_t *sem_response_client1 = sem_open(sem_response_client1_name, O_CREAT | O_EXCL, 0666, 0);

    if (sem_connect == SEM_FAILED || sem_command == SEM_FAILED || sem_response_client0 == SEM_FAILED || sem_response_client1 == SEM_FAILED) {
        perror("Failed to create semaphores");
        exit(EXIT_FAILURE);
    }

    // Initialize FIFOs
    char server_read_fifo[BUFFER_SIZE];
    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    printf("Creating FIFOs...\n");
    pipe_init(server_read_fifo);
    pipe_init(server_write_fifo);

    // Open FIFOs
    int read_fd = pipe_open_read(server_read_fifo);
    int write_fd = pipe_open_write(server_write_fifo);
    if (read_fd == -1 || write_fd == -1) {
        perror("Failed to open FIFOs");
        exit(EXIT_FAILURE);
    }

    printf("Server initialized and ready.\n");

    // Signal readiness by posting to SEM_CONNECT
    sem_post(sem_connect);
    // Cleanup resources
    close(read_fd);
    close(write_fd);
    sem_close(sem_connect);
    sem_close(sem_command);
    sem_close(sem_response_client0);
    sem_close(sem_response_client1);
}

// void cleanup_server(const char *server_name) {
//     char server_read_fifo[BUFFER_SIZE];
//     char server_write_fifo[BUFFER_SIZE];

//     snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
//     snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

//     unlink(server_read_fifo);
//     unlink(server_write_fifo);
//     pthread_mutex_destroy(&game_mutex);
//     sem_unlink(SEM_CONNECT);
// }

void cleanup_server(const char *server_name) {
    // Generate FIFO paths
    char server_read_fifo[BUFFER_SIZE];
    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    // Unlink FIFOs
    unlink(server_read_fifo);
    unlink(server_write_fifo);

    // Unlink semaphores
    char sem_connect_name[BUFFER_SIZE];
    char sem_command_name[BUFFER_SIZE];
    char sem_response_client0_name[BUFFER_SIZE];
    char sem_response_client1_name[BUFFER_SIZE];
    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);
    snprintf(sem_response_client0_name, sizeof(sem_response_client0_name), SEM_RESPONSE_TEMPLATE, server_name, 0);
    snprintf(sem_response_client1_name, sizeof(sem_response_client1_name), SEM_RESPONSE_TEMPLATE, server_name, 1);

    sem_unlink(sem_connect_name);
    sem_unlink(sem_command_name);
    sem_unlink(sem_response_client0_name);
    sem_unlink(sem_response_client1_name);
    unlink(server_read_fifo);
    unlink(server_write_fifo);
    pthread_mutex_destroy(&game_mutex);

    printf("Server resources cleaned up.\n");
}


void send_message_to_client(int client_id, const char *server_name, const char *message) {
    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    char sem_response_name[BUFFER_SIZE];
    snprintf(sem_response_name, sizeof(sem_response_name), SEM_RESPONSE_TEMPLATE, server_name, client_id);

    sem_t *sem_response = sem_open(sem_response_name, O_CREAT, 0666, 0);

    int write_fd = pipe_open_write(server_write_fifo);
    if (write_fd != -1) {
        // For connection messages (CLIENT_ID), send without prefix
        if (strncmp(message, "CLIENT_ID:", 10) == 0) {
            send_message(write_fd, message);
            printf("Server sent connection message: %s\n", message);
        } else {
            // For all other messages, add the client prefix
            char prefixed_message[BUFFER_SIZE];
            snprintf(prefixed_message, sizeof(prefixed_message), "CLIENT_%d:%s", client_id, message);
            send_message(write_fd, prefixed_message);
            sem_post(sem_response);
            printf("Server sent prefixed message: %s\n", prefixed_message);
        }
        pipe_close(write_fd);
    }

    sem_close(sem_response);
}


void handle_client_message(int client_id, const char *message, const char *server_name, GameData *game_data) {
    printf("Handling message from client %d: %s\n", client_id, message);

    // sem_t *sem_command = sem_open(SEM_COMMAND, O_CREAT, 0666, 0);
    //     if (sem_command != SEM_FAILED) {
    //         // Signal that client can input next command
    //         sem_post(sem_command);
    //         sem_close(sem_command);
    //     }
    if (strncmp(message, "SEND_BOARD", 10) == 0) {
        char *encoded_board = message + 11;// Skip "SEND_BOARD "
        printf("%s\n", encoded_board);
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

        if (sscanf(message + 7, "%d_%d", &x, &y) == 2) {
            int opponent_id = 1 - client_id; // Get opponent's ID
            GameBoard *opponent_board = &game_data->board_players[opponent_id];

            int result = attack(opponent_board, x, y);

            // Send attack result back to attacking client
            char response[BUFFER_SIZE];
            snprintf(response, sizeof(response), "ATTACK_RESULT_%c_%d_%d", result == 1 ? 'H' : 'M', x, y);
            send_message_to_client(client_id, server_name, response);

            // Notify opponent about being attacked
            snprintf(response, sizeof(response), "OPPONENT_ATTACKED_%c_%d_%d", result == 1 ? 'H' : 'M', x, y);
            send_message_to_client(opponent_id, server_name, response);
        } else {
            printf("Invalid ATTACK command format.\n");
        }
    }
}

void run_server(const char *server_name) {
    // Initialize the server
    initialize_server(server_name);

    // Generate unique semaphore names
    char sem_command_name[BUFFER_SIZE];
    char sem_response_client0_name[BUFFER_SIZE];
    char sem_response_client1_name[BUFFER_SIZE];
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);
    snprintf(sem_response_client0_name, sizeof(sem_response_client0_name), SEM_RESPONSE_TEMPLATE, server_name, 0);
    snprintf(sem_response_client1_name, sizeof(sem_response_client1_name), SEM_BOARD_TEMPLATE, server_name, 1);

    // Open semaphores for command and response handling
    sem_t *sem_command = sem_open(sem_command_name, O_CREAT, 0666, 0);
    sem_t *sem_response_client0 = sem_open(sem_response_client0_name, O_CREAT, 0666, 0);
    sem_t *sem_response_client1 = sem_open(sem_response_client1_name, O_CREAT, 0666, 0);

    if (sem_command == SEM_FAILED || sem_response_client0 == SEM_FAILED || sem_response_client1 == SEM_FAILED) {
        perror("Failed to create semaphores");
        //cleanup_server(server_name);
        exit(EXIT_FAILURE);
    }

    // Prepare FIFOs
    char server_read_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);

    // Initialize game data
    GameData game_data;
    initialize_game(&game_data);

    int connected_clients = 0;

    while (1) {
        // Open the read FIFO to receive messages from clients
        int read_fd = pipe_open_read(server_read_fifo);
        if (read_fd == -1) {
            sleep(1); // Retry if FIFO is not ready
            continue;
        }

        char buffer[BUFFER_SIZE];
        if (receive_message(read_fd, buffer, BUFFER_SIZE) == 0) {
            printf("Server received: %s\n", buffer);

            if (strncmp(buffer, "CONNECT", 7) == 0) {
                pthread_mutex_lock(&game_mutex);

                if (connected_clients < MAX_CLIENTS) {
                    int new_client_id = connected_clients;
                    char response[BUFFER_SIZE];

                    // Assign a new client ID and send it to the client
                    snprintf(response, sizeof(response), "CLIENT_ID:%d", new_client_id);
                    send_message_to_client(new_client_id, server_name, response);

                    printf("Server: Assigned ID %d to new client\n", new_client_id);

                    // Update game data for connected clients
                    if (new_client_id == 0) {
                        game_data.client_id_1 = new_client_id;
                    } else if (new_client_id == 1) {
                        game_data.client_id_2 = new_client_id;
                    }

                    connected_clients++;

                    // If both clients are connected, start the game
                    if (connected_clients == MAX_CLIENTS) {
                        // send_message_to_client(0, server_name, "GAME_START");
                        // send_message_to_client(1, server_name, "GAME_START");
                        //
                        // // Allow the first player to make a move
                        // sem_post(sem_command);
                    }
                } else {
                    char server_write_fifo[BUFFER_SIZE];
                    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

                    int write_fd = pipe_open_write(server_write_fifo);
                    char response[BUFFER_SIZE];
                    snprintf(response, sizeof(response), "REJECT");
                    send_message(write_fd, response);
                }

                pthread_mutex_unlock(&game_mutex);

            } else {
                // Handle game-related messages from clients
                int client_id;
                char message[BUFFER_SIZE];

                if (sscanf(buffer, "CLIENT_%d:%s", &client_id, message) == 2) {
                    pthread_mutex_lock(&game_mutex);
                    if (strncmp(message, "SEND_BOARD", 10) == 0) {
                        game_data.boards_ready[client_id] = 1;
                        handle_client_message(client_id, message, server_name, &game_data);

                        // Check if both boards are ready
                        if (game_data.boards_ready[0] && game_data.boards_ready[1]) {
                            printf("Both boards are ready. Notifying clients...\n");
                        }
                    } else if (strncmp(message, "ATTACK", 6) == 0) {

                        handle_client_message(client_id, message, server_name, &game_data);

                        if (client_id == 0) {
                            sem_post(sem_response_client1);
                        } else {
                            sem_post(sem_response_client0);
                        }

                    } else if (strncmp(message, "QUIT", 4) == 0) {
                        int opponent_id = 1 - client_id;

                        send_message_to_client(opponent_id, server_name, "OPPONENT_QUIT");
                        send_message_to_client(client_id, server_name, "MY_QUIT");

                        printf("Client %d quit the game. Notifying opponent and shutting down the server.\n", client_id);

                        cleanup_server(server_name);

                        printf("Server shut down successfully.\n");
                        exit(EXIT_SUCCESS);
                    }

                    pthread_mutex_unlock(&game_mutex);
                }
            }
        }

        pipe_close(read_fd);
        usleep(100000); // Small delay to avoid busy-waiting
    }

    // Cleanup resources when the server exits
    sem_close(sem_command);
    sem_close(sem_response_client0);
    sem_close(sem_response_client1);
    // cleanup_server(server_name);
}