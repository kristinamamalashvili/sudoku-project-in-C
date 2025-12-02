#include "solver.h"

// Check if value can be placed in (row, col)
int solver_is_safe(const Board b, int row, int col, int value)
{
    // Row check
    for (int c = 0; c < BOARDSIZE; c++) {
        if (b[row][c] == value)
            return 0;
    }

    // Column check
    for (int r = 0; r < BOARDSIZE; r++) {
        if (b[r][col] == value)
            return 0;
    }

    // 3×3 subgrid check
    int start_row = (row / 3) * 3;
    int start_col = (col / 3) * 3;

    for (int r = start_row; r < start_row + 3; r++) {
        for (int c = start_col; c < start_col + 3; c++) {
            if (b[r][c] == value)
                return 0;
        }
    }

    return 1; // safe
}

static int find_empty(const Board b, int *row, int *col)
{
    for (int r = 0; r < BOARDSIZE; r++) {
        for (int c = 0; c < BOARDSIZE; c++) {
            if (b[r][c] == 0) {
                *row = r;
                *col = c;
                return 1;
            }
        }
    }
    return 0; // no empty cells → board is full
}

int solve_board(Board b) {
    int row, col;

    if (!find_empty(b, &row, &col))
        return 1;

    for (int value = 1; value <= 9; value++) {
        if (solver_is_safe(b, row, col,value)){
            b[row][col] = value;

            if (solve_board(b))
                return 1;

            b[row][col] = 0;
        }
    }

    return 0;
}

bool solve(Board b)
{
    return solve_board(b) != 0;
}