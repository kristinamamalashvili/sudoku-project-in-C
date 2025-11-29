#ifndef SOLVER_H
#define SOLVER_H

#include "board.h"    // for Board type

// Solve the Sudoku board using backtracking.
// Returns 1 if solved, 0 if no solution exists.
int solve_board(Board *b);

// Check if placing value at (row, col) obeys Sudoku rules.
// Used internally but can also be used by generator.
int solver_is_safe(const Board *b, int row, int col, int value);


#endif SOLVER_H