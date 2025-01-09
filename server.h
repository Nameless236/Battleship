#ifndef SERVER_H
#define SERVER_H

#include "game-logic.h"
#include <pthread.h>

#define MAX_CLIENTS 2

typedef struct {
    GameBoard board_players[MAX_CLIENTS];
    int player_turn;
    int client_id_1;
    int client_id_2;
    int boards_ready[2];
    int game_started;
} GameData;


void initialize_server(const char *server_name);
void cleanup_server(const char *server_name);
void run_server(const char *server_name);
void handle_client_message(int client_id, const char *message, const char *server_name, GameData *game_data);
void send_message_to_client(int client_id, const char *server_name, const char *message) ;
void handle_board_message(int client_id, const char *message, GameData *game_data);
void initialize_game(GameData *game_data);

#endif
