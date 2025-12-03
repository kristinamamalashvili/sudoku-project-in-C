#ifndef SOLVER_H
#define SOLVER_H

#include "board.h"
#include <stdbool.h>

bool solve(Board b);

int solve_board(Board b);

int solver_is_safe(const Board b, int row, int col, int value);


#endif //SOLVER_H



