#include "board.h" // Assumes board.c is in 'src' and board.h is in 'include'
#include <stdlib.h> // Good practice for general C

// Initializes board to all zeros
void board_init(Board b) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            b[i][j] = 0;
        }
    }
}

// Prints the board nicely to the specified stream
void board_print(const Board b, FILE *stream) {
    //
    fprintf(stream, "\n+-------+-------+-------+\n");
    for (int i = 0; i < SIZE; i++) {
        fprintf(stream, "| ");
        for (int j = 0; j < SIZE; j++) {
            if (b[i][j] == 0) {
                fprintf(stream, "  ");
            } else {
                fprintf(stream, "%d ", b[i][j]);
            }
            if ((j + 1) % 3 == 0) {
                fprintf(stream, "| ");
            }
        }
        fprintf(stream, "\n");
        if ((i + 1) % 3 == 0) {
            fprintf(stream, "+-------+-------+-------+\n");
        }
    }
}

// Checks Sudoku rules for a move (r, c, v)
bool board_is_move_valid(const Board b, int r, int c, int v) {
    // 1. Sanity Check
    if (r < 0 || r >= SIZE || c < 0 || c >= SIZE || v < 1 || v > 9) {
        return false;
    }

    // 2. Check Row and Column
    for (int i = 0; i < SIZE; i++) {
        if (b[r][i] == v) { return false; } // Row conflict
        if (b[i][c] == v) { return false; } // Column conflict
    }

    // 3. Check 3x3 Box
    int box_start_row = r - r % 3;
    int box_start_col = c - c % 3;

    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            if (b[box_start_row + i][box_start_col + j] == v) {
                return false;
            }
        }
    }

    return true;
}

// Checks if the board is completely filled
bool board_is_full(const Board b) {
    for (int i = 0; i < SIZE; i++) {
        for (int j = 0; j < SIZE; j++) {
            if (b[i][j] == 0) {
                return false;
            }
        }
    }
    return true;
}
