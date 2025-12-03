#ifndef BOARD_H
#define BOARD_H

#include <stdio.h>
#include <stdbool.h>

#define BOARDSIZE 9

typedef int Board[BOARDSIZE][BOARDSIZE];

void board_init(Board b);
void board_print(const Board b, FILE *stream);
bool board_is_move_valid(const Board b, int r, int c, int v);
bool board_is_full(const Board b);
void board_to_string(const Board b, char *buf, size_t buf_size);


#endif // BOARD_H



