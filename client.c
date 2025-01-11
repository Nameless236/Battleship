#include <semaphore.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include "client.h"
#include "communication.h"
#include "game-logic.h"
#include "pipe.h"
#include "config.h"
#include "server.h"
#include <errno.h>
#include <stdbool.h>

int quit_pipe[2]; // Global pipe for signaling quit

void initialize_quit_pipe() {
    if (pipe(quit_pipe) == -1) {
        perror("Failed to create quit pipe");
        exit(EXIT_FAILURE);
    }
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

int run_client(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <server_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    const char *server_name = argv[1];
    ThreadArgs args = {0};

    initialize_quit_pipe();

    // Setup communication and initialize game state
    setup_communication(server_name, &args);

    // Connect to server and handle threads
    connect_to_server(&args);

    char sem_response_name[BUFFER_SIZE];
    char client_read_fifo[BUFFER_SIZE];

    snprintf(client_read_fifo, sizeof(client_read_fifo), CLIENT_READ_FIFO_TEMPLATE, server_name, args.client_id);
    snprintf(sem_response_name, sizeof(sem_response_name), SEM_RESPONSE_TEMPLATE, server_name, args.client_id);

    char sem_continue_name[BUFFER_SIZE];
    snprintf(sem_continue_name, sizeof(sem_continue_name), SEM_CONTINUE_TEMPLATE, server_name, args.client_id);

    args.sem_continue = sem_open(sem_continue_name, O_RDWR);
    int read_fd_client = pipe_open_read(client_read_fifo);
    if (read_fd_client == -1) {
        perror("Failed to open pipes");
        exit(EXIT_FAILURE);
    }
    args.read_fd = read_fd_client;
    args.sem_response = sem_open(sem_response_name, O_RDWR);

    handle_client_threads(&args);

    // Cleanup resources after threads finish
    cleanup_resources(&args);

    return 0;
}

void initialize_client_game_state(ClientGameState *state) {
    initialize_board(&state->my_board);
    initialize_board(&state->enemy_board);
    initialize_fleet(&state->fleet);
    state->ships_to_place = 2;  // Set initial number of ships
    atomic_init(&state->game_over, false);
    state->board_ready = 0;
}

void create_server_process(const char *server_name) {
    pid_t pid = fork();
    if (pid == 0) {
        run_server(server_name); // Child process: Start the server

        wait(NULL);

        exit(EXIT_SUCCESS);
    } else if (pid > 0) {
        printf("Server process created with PID: %d\n", pid);
    } else {
        perror("Failed to create server process");
        exit(EXIT_FAILURE);
    }
}

void setup_communication(const char *server_name, ThreadArgs *args) {
    char sem_connect_name[BUFFER_SIZE], sem_command_name[BUFFER_SIZE];
    snprintf(sem_connect_name, sizeof(sem_connect_name), SEM_CONNECT_TEMPLATE, server_name);
    snprintf(sem_command_name, sizeof(sem_command_name), SEM_COMMAND_TEMPLATE, server_name);

    char server_read_fifo[BUFFER_SIZE], server_write_fifo[BUFFER_SIZE];
    snprintf(server_read_fifo, sizeof(server_read_fifo), SERVER_READ_FIFO_TEMPLATE, server_name);
    snprintf(server_write_fifo, sizeof(server_write_fifo), SERVER_WRITE_FIFO_TEMPLATE, server_name);



    // Check if FIFOs exist; if not, create a new server process
    if (access(server_read_fifo, F_OK) == -1 || access(server_write_fifo, F_OK) == -1) {
        sem_unlink(sem_connect_name);
        sem_t *sem_connect = sem_open(sem_connect_name, O_CREAT | O_EXCL, 0666, 0);
        if (sem_connect == SEM_FAILED) {
            perror("Failed to open SEM_CONNECT semaphore");
            exit(EXIT_FAILURE);
        }

        printf("Server does not exist. Creating a new server...\n");
        create_server_process(server_name);

        printf("Waiting for server initialization...\n");
        sem_wait(sem_connect);
        sem_close(sem_connect);
    }

    printf("Server is ready. Connecting...\n");

    args->sem_command = sem_open(sem_command_name, O_CREAT | O_EXCL, 0666, 0);
    if (args->sem_command == SEM_FAILED) {
        if (errno == EEXIST) {
            // Semaphore already exists; open it without O_CREAT
            args->sem_command = sem_open(sem_command_name, 0);
            if (args->sem_command == SEM_FAILED) {
                perror("Failed to open existing command semaphore");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("Failed to open command semaphore");
            exit(EXIT_FAILURE);
        }
    }

    args->write_fd = pipe_open_write(server_read_fifo);
    args->read_fd = pipe_open_read(server_write_fifo);

    if (args->write_fd == -1 || args->read_fd == -1) {
        perror("Failed to open pipes");
        exit(EXIT_FAILURE);
    }

    args->game_state = malloc(sizeof(ClientGameState));
    if (!args->game_state) {
        perror("Failed to allocate memory for game state");
        exit(EXIT_FAILURE);
    }

    initialize_client_game_state(args->game_state);
}

void cleanup_resources(ThreadArgs *args) {
    if (args->sem_command != NULL && sem_close(args->sem_command) == -1) {
        perror("Failed to close command semaphore");
    }

    if (args->sem_response != NULL && sem_close(args->sem_response) == -1) {
        perror("Failed to close response semaphore");
    }

    free(args->game_state);

    pipe_close(args->write_fd);
    pipe_close(args->read_fd);

    printf("Resources cleaned up successfully.\n");
}

void connect_to_server(ThreadArgs *args) {
    send_message(args->write_fd, "CONNECT");
    sem_post(args->sem_command);

    char buffer[BUFFER_SIZE];

    while (1) {
        if (receive_message(args->read_fd, buffer, BUFFER_SIZE) == 0 && strncmp(buffer, "CLIENT_ID:", 10) == 0) {
            sscanf(buffer + 10, "%d", &args->client_id);
            printf("Successfully connected with ID: %d\n", args->client_id);
            break;
        } else if (strncmp(buffer, "REJECT", 6) == 0) {
            printf("Connection rejected by the server. The game is full.\n");
            cleanup_resources(args); // Cleanup before exiting
            exit(EXIT_SUCCESS);
        }

        usleep(100000); // Retry after delay
    }
}

void handle_client_threads(ThreadArgs *args) {
    pthread_t command_thread, update_thread;
    if (!args || !args->game_state || !args->sem_command || !args->sem_response) {
        fprintf(stderr, "Invalid thread arguments\n");
        exit(EXIT_FAILURE);
    }

    pthread_create(&command_thread, NULL, handle_commands, args);
    pthread_create(&update_thread, NULL, handle_updates, args);

    pthread_join(command_thread, NULL);
    pthread_join(update_thread, NULL);
    printf("soooom\n");
}

void signal_quit() {
    write(quit_pipe[1], "Q", 1); // Write to the pipe to signal quit
}

void *handle_commands(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    if (!args || !args->game_state || !args->sem_command) {
        fprintf(stderr, "Invalid arguments in handle_commands\n");
        pthread_exit(NULL);
    }

    place_ships(args->game_state, args); // Handles ship placement

    char buffer[BUFFER_SIZE];

    while (!atomic_load(&args->game_state->game_over)) { // Check game_over flag
        printf("\nEnter command (ATTACK x y / QUIT): ");

        // Set up file descriptor set for select
        fd_set read_fds;
        FD_ZERO(&read_fds);
        FD_SET(STDIN_FILENO, &read_fds); // Monitor stdin (file descriptor 0)
        FD_SET(quit_pipe[0], &read_fds); // Monitor quit_pipe read end

        int max_fd = quit_pipe[0] > STDIN_FILENO ? quit_pipe[0] : STDIN_FILENO;

        int result = select(max_fd + 1, &read_fds, NULL, NULL, NULL);
        if (result > 0) {
            if (FD_ISSET(quit_pipe[0], &read_fds)) {
                printf("Quit signal received. Exiting command thread.\n");
                break; // Exit thread on quit signal
            }

            if (FD_ISSET(STDIN_FILENO, &read_fds)) {
                if (fgets(buffer, sizeof(buffer), stdin) != NULL) {
                    if (strncmp(buffer, "ATTACK", 6) == 0) {
                        int x, y;
                        if (sscanf(buffer, "ATTACK %d %d", &x, &y) == 2) {
                            snprintf(buffer, sizeof(buffer), "CLIENT_%d:ATTACK_%d_%d", args->client_id, x, y);
                            send_message(args->write_fd, buffer);
                            sem_post(args->sem_command); // Notify server of new command
                            printf("Attack sent. Waiting for result...\n");
                        } else {
                            printf("Invalid input. Use: ATTACK x y\n");
                        }
                    } else if (strncmp(buffer, "QUIT", 4) == 0) {
                        snprintf(buffer, sizeof(buffer), "CLIENT_%d:QUIT", args->client_id);
                        send_message(args->write_fd, buffer);
                        sem_post(args->sem_command); // Notify server of quit command
                        printf("Quitting the game...\n");
                        atomic_store(&args->game_state->game_over, true); // Signal game over
                        break;
                    } else {
                        printf("Unknown command. Try again.\n");
                    }
                }
            }
        } else if (result < 0) {
            perror("Error in select");
            break;
        }
    }

    printf("Exiting command thread.\n");
    return NULL;
}


void *handle_updates(void *arg) {
    ThreadArgs *args = (ThreadArgs *)arg;

    char buffer[BUFFER_SIZE];
    while (!atomic_load(&args->game_state->game_over)) { // Check game_over flag
        sem_wait(args->sem_response);

        if (receive_message(args->read_fd, buffer, BUFFER_SIZE) == 0) {
            process_server_message(args, buffer); // Handle different message types

            // Set game_over flag if GAME_OVER or OPPONENT_QUIT is received
            if (strstr(buffer, "GAME_OVER") != NULL || strstr(buffer, "OPPONENT_QUIT") != NULL) {

                atomic_store(&args->game_state->game_over, true); // Signal game over
                sem_post(args->sem_continue);
                write(quit_pipe[1], "Q", 1); // Write to the pipe to signal quit
                printf("Game over or opponent quit detected. Exiting update thread.\n");
                pthread_exit(NULL); // Exit this thread
            }
        } else {
            perror("Failed to receive message from server");
        }
    }

    return NULL;
}


void process_server_message(ThreadArgs *args, const char *buffer) {
    char expected_prefix[BUFFER_SIZE];
    snprintf(expected_prefix, sizeof(expected_prefix), "CLIENT_%d:", args->client_id);

    if (strncmp(buffer, expected_prefix, strlen(expected_prefix)) == 0) {
        const char *message = buffer + strlen(expected_prefix);
        printf("Received message: %s\n", message);

        if (strncmp(message, "BOARD_RECEIVED", 13) == 0) {
            print_boards(&args->game_state->my_board, &args->game_state->enemy_board);
            printf("\nEnter ATTACK placement (ATTACK x y) or QUIT: ");
        } else if (strncmp(message, "ATTACK_RESULT", 13) == 0) {
            int x, y;
            char result;

            if (sscanf(message + 14, "%c_%d_%d", &result, &x, &y) == 3) {
                if (result == 'H') {
                    printf("You hit a ship at (%d, %d)!\n", x, y);
                    args->game_state->enemy_board.grid[y][x] = 2; // Mark hit
                } else if (result == 'M') {
                    printf("You missed at (%d, %d).\n", x, y);
                    args->game_state->enemy_board.grid[y][x] = 3; // Mark miss
                }
            }
            sem_post(args->sem_continue);
            print_boards(&args->game_state->my_board, &args->game_state->enemy_board);
        } else if (strncmp(message, "OPPONENT_ATTACKED", 17) == 0) {
            int x, y;
            char result;

            if (sscanf(message + 18, "%c_%d_%d", &result, &x, &y) == 3) {
                if (result == 'H') {
                    printf("You hit a ship at (%d, %d)!\n", x, y);
                    args->game_state->my_board.grid[y][x] = 2; // Zásah)
                } else if (result == 'M') {
                    printf("You missed at (%d, %d).\n", x, y);
                    args->game_state->my_board.grid[y][x] = 3; // Minutie
                }
            }
            sem_post(args->sem_continue);
            printf("Opponent attacked at (%d, %d).\n", x, y);
            print_boards(&args->game_state->my_board, &args->game_state->enemy_board);
        } else if (strncmp(message, "OPPONENT_QUIT", 13) == 0) {
            atomic_store(&args->game_state->game_over, true); // Signal game over
            write(quit_pipe[1], "Q", 1); // Write to the pipe to signal quit
            printf("Opponent quit detected.\n");
        } else if (strncmp(message, "MY_QUIT", 6) == 0) {
            printf("I have quit the game. I lose!\n");

            atomic_store(&args->game_state->game_over, true); // Signal game over
            write(quit_pipe[1], "Q", 1); // Write to the pipe to signal quit
            printf("My quit detected.\n");
        } else if (strncmp(message, "GAME_OVER", 9) == 0) {
            atomic_store(&args->game_state->game_over, true); // Signal game over
            write(quit_pipe[1], "Q", 1); // Write to the pipe to signal quit
            printf("Game over or opponent quit detected.\n");
        } else if (strncmp(message, "WRONG_TURN", 10) == 0) {
            printf("IT'S YOUR OPPONENTS TURN.");
        }
    } else {
        printf("Unexpected message format: %s\n", buffer);
    }
}


void place_ships(ClientGameState *game_state, ThreadArgs *args) {
    char buffer[BUFFER_SIZE];
    int index = 0;

    initialize_fleet(&game_state->fleet);

    while (index < 1) {
        system("clear");
        print_fleet(&game_state->fleet, game_state->ships_to_place - index);
        printf("\nYour current board:\n");
        print_board(&game_state->my_board);

        printf("\nPlacing ship: %s (Size: %d)\n", game_state->fleet.ships[index].name, game_state->fleet.ships[index].size);
        printf("Ships remaining: %d\n", game_state->ships_to_place - index);
        printf("\nEnter ship placement (PLACE x y orientation): ");
        fgets(buffer, sizeof(buffer), stdin);

        int x, y;
        char orientation;
        if (sscanf(buffer, "PLACE %d %d %c", &x, &y, &orientation) == 3) {
            if (place_ship_from_fleet(&game_state->my_board, x, y, &game_state->fleet.ships[4], orientation)) {
                printf("Ship placed successfully!\n");
                index++;
            } else {
                printf("Failed to place ship. Invalid position or overlap. Try again.\n");
            }
        } else {
            printf("Invalid input. Please use the format: PLACE x y orientation\n");
            sleep(2);
        }
    }

    // Notify server that all ships are placed
    send_board_to_server(args->write_fd, args->client_id, &game_state->my_board);
    sem_post(args->sem_command);
}

void handle_gameplay_commands(ThreadArgs *args) {
    char buffer[BUFFER_SIZE];

    while (1) {
        printf("\nEnter command (ATTACK x y / QUIT): ");
        fgets(buffer, sizeof(buffer), stdin);

        if (strncmp(buffer, "ATTACK", 6) == 0) {
            int x, y;
            if (sscanf(buffer, "ATTACK %d %d", &x, &y) == 2) {
                snprintf(buffer, sizeof(buffer), "CLIENT_%d:ATTACK_%d_%d", args->client_id, y, x);
                send_message(args->write_fd, buffer);
                sem_post(args->sem_command);
                printf("Attack sent. Waiting for result...\n");
            } else {
                printf("Invalid input. Use: ATTACK x y\n");
            }
        } else if (strncmp(buffer, "QUIT", 4) == 0) {
            snprintf(buffer, sizeof(buffer), "CLIENT_%d:QUIT", args->client_id);
            send_message(args->write_fd, buffer);
            sem_post(args->sem_command);
            printf("Quitting the game...\n");
            return;
        } else {
            printf("Unknown command. Try again.\n");
        }
    }
}
