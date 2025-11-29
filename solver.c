#include "solver.h"

// Check if value can be placed in (row, col)
int solver_is_safe(const Board *b, int row, int col, int value)
{
    // Row check
    for (int c = 0; c < BOARD_SIZE; c++) {
        if (b->cells[row][c] == value)
            return 0;
    }

    // Column check
    for (int r = 0; r < BOARD_SIZE; r++) {
        if (b->cells[r][col] == value)
            return 0;
    }

    // 3×3 subgrid check
    int start_row = (row / 3) * 3;
    int start_col = (col / 3) * 3;

    for (int r = start_row; r < start_row + 3; r++) {
        for (int c = start_col; c < start_col + 3; c++) {
            if (b->cells[r][c] == value)
                return 0;
        }
    }

    return 1; // safe
}

// Find an empty cell (0)
// Returns 1 and stores row/col, OR returns 0 if none are empty
static int find_empty(const Board *b, int *row, int *col)
{
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (b->cells[r][c] == 0) {
                *row = r;
                *col = c;
                return 1;
            }
        }
    }
    return 0; // no empty cells → board is full
}

int solve_board(Board *b)
{
    int row, col;

    // If no empty cell → solved
    if (!find_empty(b, &row, &col))
        return 1;

    // Try values 1 through 9
    for (int value = 1; value <= 9; value++) {
        if (solver_is_safe(b, row, col, value)) {
            b->cells[row][col] = value;

            // Recursive attempt
            if (solve_board(b))
                return 1;

            // Backtrack
            b->cells[row][col] = 0;
        }
    }

    return 0; // no value fits → backtrack
}
