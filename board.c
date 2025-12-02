#include "board.h"
#include <stdlib.h>
#include <stdio.h>

void board_init(Board b) {
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            b[i][j] = 0;
        }
    }
}

// Prints the board nicely to the specified stream
void board_print(const Board b, FILE *stream) {

    fprintf(stream, "\n    ");
    for (int col = 0; col < BOARDSIZE; col++) {
        fprintf(stream, "%d ", col + 1);
        if ((col + 1) % 3 == 0)
            fprintf(stream, "  ");
    }

    fprintf(stream, "\n+-------+-------+-------+\n");
    for (int i = 0; i < BOARDSIZE; i++) {
        fprintf(stream, "%c | ", 'A' + i);
        for (int j = 0; j < BOARDSIZE; j++) {
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
    if (r < 0 || r >= BOARDSIZE || c < 0 || c >= BOARDSIZE || v < 1 || v > 9) {
        return false;
    }

    // 2. Check Row and Column
    for (int i = 0; i < BOARDSIZE; i++) {
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
    for (int i = 0; i < BOARDSIZE; i++) {
        for (int j = 0; j < BOARDSIZE; j++) {
            if (b[i][j] == 0) {
                return false;
            }
        }
    }
    return true;
}


// New function to write the board to a string buffer for networking
void board_to_string(const Board b, char *buf, size_t buf_size)
{
    size_t pos = 0;
    int n;

#define APP(...)                                                            \
do {                                                                    \
if (pos < buf_size) {                                               \
n = snprintf(buf + pos, buf_size - pos, __VA_ARGS__);           \
if (n < 0) return;                                              \
if ((size_t)n >= buf_size - pos) {                              \
/* Truncated: terminate and stop */                         \
pos = buf_size - 1;                                         \
buf[pos] = '\0';                                            \
return;                                                     \
}                                                               \
pos += (size_t)n;                                               \
}                                                                   \
} while (0)

    // Column header
    APP("\n    ");
    for (int col = 0; col < BOARDSIZE; col++) {
        APP("%d ", col + 1);
        if ((col + 1) % 3 == 0)
            APP("  ");
    }
    APP("\n+-------+-------+-------+\n");

    // Rows
    for (int i = 0; i < BOARDSIZE; i++) {
        APP("%c | ", 'A' + i);
        for (int j = 0; j < BOARDSIZE; j++) {
            if (b[i][j] == 0) {
                APP("  ");
            } else {
                APP("%d ", b[i][j]);
            }
            if ((j + 1) % 3 == 0)
                APP("| ");
        }
        APP("\n");
        if ((i + 1) % 3 == 0) {
            APP("+-------+-------+-------+\n");
        }
    }

    buf[pos] = '\0';
#undef APP
}

