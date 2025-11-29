#ifndef BOARD_H
#define BOARD_H

#include <stdio.h>    // For FILE* in board_print
#include <stdbool.h>  // For true/false return types

#define SIZE 9

typedef int Board[SIZE][SIZE];

void board_init(Board b);
void board_print(const Board b, FILE *stream);
bool board_is_move_valid(const Board b, int r, int c, int v);
bool board_is_full(const Board b);


#endif BOARD_H