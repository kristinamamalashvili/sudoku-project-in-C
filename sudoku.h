
#ifndef SUDOKU_H
#define SUDOKU_H

#include "board.h"

typedef enum {
    MODE_SERVER,
    MODE_CLIENT
} ProgramMode;

ProgramMode parse_mode(int argc, char *argv[], int *out_player_id);
int run_server(void);
int run_client(int player_id);

#endif //SUDOKU_H