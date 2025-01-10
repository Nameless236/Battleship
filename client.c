#include "client.h"
#include "communication.h"
#include "game-logic.h"
#include "pipe.h"
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <semaphore.h>
#include <fcntl.h>
#include <stdbool.h>
#include "server.h"
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "config.h"

// In client.h
typedef struct {
    GameBoard my_board;
    GameBoard enemy_board;
    int ships_to_place;
    int board_ready;
} ClientGameState;

typedef struct {
    int write_fd;
    int read_fd;
    int client_id;
    ClientGameState *game_state;
    sem_t *sem_command;    // For sending commands
    sem_t *sem_response;   // For reading responses
} ThreadArgs;

void initialize_client_game_state(ClientGameState *state) {
    initialize_board(&state->my_board);
    initialize_board(&state->enemy_board);
    state->ships_to_place = 2;  // Set initial number of ships
    state->board_ready = 0;
}

// void *handle_updates(void *arg) {
//     ThreadArgs *args = (ThreadArgs *)arg;
//     char buffer[BUFFER_SIZE];

//     while (1) {
//         if (receive_message(args->read_fd, buffer, BUFFER_SIZE) == 0) {
//             printf("%s\n", buffer);
//             char expected_prefix[BUFFER_SIZE];
//             snprintf(expected_prefix, sizeof(expected_prefix), "CLIENT_%d:", args->client_id);

//             if (strncmp(buffer, expected_prefix, strlen(expected_prefix)) == 0) {
//                 char *message = buffer + strlen(expected_prefix);
//                 printf("Server: %s\n", message);

//                 if (strncmp(message, "PLACE_RESULT 1", 14) == 0) {
//                     printf("Ship placed successfully!\n");
//                 } else if (strncmp(message, "PLACE_RESULT 0", 14) == 0) {
//                     printf("Failed to place ship. Try again.\n");
//                 } else if (strcmp(message, "GAME_START") == 0) {
//                     printf("Game is starting! All players connected.\n");
//                 }
//             }
//         }
//         usleep(100000);
//     }
//     return NULL;
// }

void create_server_process(const char *server_name) {
    pid_t pid = fork();

    if (pid == 0) {
        // Child process: Start the server
        run_server(server_name);
    } else if (pid > 0) {
        // Parent process: Log success
        printf("Server process created with PID: %d\n", pid);
    } else {
        // Fork failed
        perror("Failed to create server process");
        exit(EXIT_FAILURE);
    }
}

// bool file_exists(const char *filename) {
//     struct stat buffer;
//     return (stat(filename, &buffer) == 0);
// }

void run_client(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_name = argv[1];

    // Generate unique semaphore names
    char sem_connect_name[BUFFER_SIZE];
    char sem_command_name[BUFFER_SIZE];
    char sem_response_name[BUFFER_SIZE];
    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);

    sem_unlink(sem_connect_name);
    sem_t *sem_connect = sem_open(sem_connect_name, O_CREAT | O_EXCL, 0666, 0);

    // Open SEM_CONNECT semaphore to wait for server readiness

    if (sem_connect == SEM_FAILED) {
        perror("Failed to open SEM_CONNECT semaphore");
        exit(EXIT_FAILURE);
    }

    // Check if the server FIFOs exist; if not, create a new server process
    char server_read_fifo[BUFFER_SIZE];
    char server_write_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);

    if (access(server_read_fifo, F_OK) == -1 || access(server_write_fifo, F_OK) == -1) {
        printf("Server does not exist. Creating a new server...\n");
        create_server_process(server_name);

        printf("Waiting for server initialization...\n");
        sem_wait(sem_connect);
    }

    printf("Server is ready. Connecting...\n");

    // Open FIFOs for communication
    int write_fd = pipe_open_write(server_read_fifo);
    int read_fd = pipe_open_read(server_write_fifo);

    if (write_fd == -1 || read_fd == -1) {
        perror("Failed to open pipes");
        exit(EXIT_FAILURE);
    }
    // Send connection request
    if (send_message(write_fd, "CONNECT") != 0) {
        perror("Failed to send connection request");
        pipe_close(write_fd);
        pipe_close(read_fd);
        exit(EXIT_FAILURE);
    }

    ClientGameState *game_state = malloc(sizeof(ClientGameState));
    initialize_client_game_state(game_state);

    // Open command and response semaphores
    sem_t *sem_command = sem_open(sem_command_name, O_CREAT, 0666, 0);

    if (sem_command == SEM_FAILED) {
        perror("Failed to open command/response semaphores");
        free(game_state);
        pipe_close(write_fd);
        pipe_close(read_fd);
        sem_close(sem_connect);
        exit(EXIT_FAILURE);
    }

    // Prepare thread arguments
    ThreadArgs thread_args = {
        .write_fd = write_fd,
        .read_fd = read_fd,
        .client_id = -1,
        .game_state = game_state,
        .sem_command = sem_command,
        .sem_response = NULL
    };

    // Wait for CLIENT_ID from the server
    char buffer[BUFFER_SIZE];
    int client_id = -1;
    int retry_count = 0;
    const int MAX_RETRIES = 10;

    while (retry_count < MAX_RETRIES && client_id == -1) {
        int result = receive_message(read_fd, buffer, BUFFER_SIZE);
        printf("%s\n", buffer);
        if (result == 0 && strncmp(buffer, "CLIENT_ID:", 10) == 0 ){
                sscanf(buffer + 10, "%d", &client_id);
                thread_args.client_id = client_id;

                snprintf(sem_response_name, sizeof(sem_response_name), SEM_RESPONSE_TEMPLATE, server_name, client_id);
                sem_t *sem_response = sem_open(sem_response_name, O_CREAT, 0666, 0);
                if (sem_response == SEM_FAILED) {
                    perror("Failed to open response semaphores");
                    free(game_state);
                    pipe_close(write_fd);
                    pipe_close(read_fd);
                    sem_close(sem_response);
                    exit(EXIT_FAILURE);
                }
                printf("Sem_responce_name: %s\n", sem_response_name);
                thread_args.sem_response = sem_response;

                printf("Successfully connected with ID: %d\n", client_id);

                pthread_t command_thread, update_thread;
                pthread_create(&command_thread, NULL, handle_commands, &thread_args);
                pthread_create(&update_thread, NULL, handle_updates, &thread_args);

                pthread_join(command_thread, NULL);
                pthread_join(update_thread, NULL);

                break;

        } else if (strncmp(buffer, "REJECT", 6) == 0) {
            printf("Connection rejected by the server. The game is full.\n");
            pipe_close(write_fd);
            pipe_close(read_fd);
            sem_close(sem_connect);
            exit(EXIT_SUCCESS); // Exit gracefully since rejection is not an error
        } else {
            printf("Unexpected response from the server: %s\n", buffer);
        }
        retry_count++;
        usleep(100000); // Retry after a short delay
    }

    // Cleanup resources
    sem_close(thread_args.sem_command);
    sem_close(thread_args.sem_response);

    free(game_state);

    pipe_close(write_fd);
    pipe_close(read_fd);

    if (client_id == -1) {
        printf("Failed to receive server acknowledgment\n");
        exit(EXIT_FAILURE);
    }
}



void *handle_commands(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    ClientGameState *game_state = args->game_state;
    char buffer[BUFFER_SIZE];
    // Print the current state of the player's board

    while (game_state->ships_to_place > 0) {
        system("clear");
        printf("\nYour current board:\n");
        print_board(&game_state->my_board);
        // Prompt the user to place a ship
        printf("Ships remaining %d\n", game_state->ships_to_place);
        printf("\nEnter ship placement (PLACE x y length orientation): ");

        fgets(buffer, sizeof(buffer), stdin);

        // Parse and validate the input
        int x, y, length;
        char orientation;
        if (sscanf(buffer, "PLACE %d %d %d %c", &x, &y, &length, &orientation) == 4) {
            // Attempt to place the ship locally on the client's board
            int result = place_ship(&game_state->my_board, x, y, length, orientation);
            if (result == 1) {
                printf("Ship placed successfully!\n");
                game_state->ships_to_place--; // Decrement remaining ships to place

                // Notify the server about the successful placement
                snprintf(buffer, sizeof(buffer), "CLIENT_%d:PLACE %d %d %d %c",
                         args->client_id, x, y, length, orientation);
            } else {
                printf("Failed to place ship. Invalid position or overlap. Try again.\n");
            }
        } else {
            printf("Invalid input. Please use the format: PLACE x y length orientation\n");
        }
    }

    // Notify that all ships have been placed
    printf("\nAll ships placed! Notifying server...\n");
    // snprintf(buffer, sizeof(buffer), "CLIENT_%d:BOARD_READY", args->client_id);
    // send_message(args->write_fd, buffer);

    // Mark the board as ready
    game_state->board_ready = 1;

    send_board_to_server(args->write_fd, args->client_id, &game_state->my_board);


    while(1) {
        printf("Handle commands: after all ships are ready \n");

        printf("\nEnter ATTACK placement (ATTACK x y) or QUIT: ");

        fgets(buffer, sizeof(buffer), stdin);

         if (strncmp(buffer, "ATTACK", 6) == 0) {
            int x, y;
            if (sscanf(buffer, "ATTACK %d %d", &x, &y) == 2) {
                // Notify the server about the attack
                snprintf(buffer, sizeof(buffer), "CLIENT_%d:ATTACK_%d_%d", args->client_id, x, y);
                send_message(args->write_fd, buffer);

                // Wait for the server's response
                printf("Waiting for attack result...\n");
            } else {
                printf("Invalid input. Use: ATTACK x y\n");
            }
        }
        // Handle quit command
        if (strncmp(buffer, "QUIT", 4) == 0) {
            printf("Quitting the game...\n");

            snprintf(buffer, sizeof(buffer), "CLIENT_%d:QUIT", args->client_id);
            send_message(args->write_fd, buffer);
            break;
        }
    }

    return NULL;
}


void *handle_updates(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("Wait before handle update message... \n");

        if (sem_wait(args->sem_response) == -1) {
            perror("Failed to wait for response signal from client");
            continue;
        }

        if (receive_message(args->read_fd, buffer, BUFFER_SIZE) == 0) {
            printf("Receive message: %s\n", buffer);
            char expected_prefix[BUFFER_SIZE];
            snprintf(expected_prefix, sizeof(expected_prefix), "CLIENT_%d:", args->client_id);
            printf("Expected prefix: %s\n", expected_prefix);
            if (strncmp(buffer, expected_prefix, strlen(expected_prefix)) == 0) {
                char *message = buffer + strlen(expected_prefix);
                printf("Received message: %s\n", message);

                // Spracovanie správy ALL_BOARDS_READY
                if (strncmp(message, "BOARD_RECEIVED", 13) == 0) {
                    print_boards(&args->game_state->my_board, &args->game_state->enemy_board);
                }
                // Spracovanie správy ATTACK_RESULT
                else if (strncmp(message, "ATTACK_RESULT", 13) == 0) {
                    int x, y;
                    char result;

                    if (sscanf(message + 14, "%c_%d_%d", &result, &x, &y) == 3) {
                        if (result == 'H') {
                            printf("You hit a ship at (%d, %d)!\n", x, y);
                            args->game_state->enemy_board.grid[x][y] = 2; // Zásah)
                        } else if (result == 'M') {
                            printf("You missed at (%d, %d).\n", x, y);
                            args->game_state->enemy_board.grid[x][y] = 3; // Minutie
                        }
                    }
                    print_boards(&args->game_state->my_board, &args->game_state->enemy_board);
                } else if (strncmp(message, "OPPONENT_QUIT", 13) == 0) {
                    printf("Your opponent has quit the game. You win!\n");

                    sem_close(args->sem_command);
                    sem_close(args->sem_response);
                    free(args->game_state);
                    pipe_close(args->write_fd);
                    pipe_close(args->read_fd);

                    printf("Exiting client...\n");
                    exit(EXIT_SUCCESS);
                } else if (strncmp(message, "MY_QUIT", 6) == 0) {
                    printf("I have quit the game. I lose!\n");

                    sem_close(args->sem_command);
                    sem_close(args->sem_response);
                    free(args->game_state);
                    pipe_close(args->write_fd);
                    pipe_close(args->read_fd);

                    printf("Exiting client...\n");
                    exit(EXIT_SUCCESS);
                }
            } else {
                printf("Unexpected message format: %s\n", buffer);
            }
        } else {
            perror("Failed to receive message from server");
        }
    }
    return NULL;
}

void send_board_to_server(int write_fd, int client_id, GameBoard *board) {
    char buffer[BUFFER_SIZE];
    char serialized_board[BOARD_SIZE * BOARD_SIZE + 1]; // For a 10x10 board + null terminator
    int index = 0;

    // Serialize the board into a single string
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            serialized_board[index++] = (board->grid[i][j] == 0) ? 'A' : 'B';
        }
    }
    serialized_board[index] = '\0'; // Null-terminate the string

    // Send the serialized board to the server
    snprintf(buffer, sizeof(buffer), "CLIENT_%d:SEND_BOARD-%s", client_id, serialized_board);
    send_message(write_fd, buffer);

    printf("Board sent to server:\n");
    print_board(board); // Print the board for debugging
}