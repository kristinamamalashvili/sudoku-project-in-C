#include "generator.h"
#include <stdio.h>
#include <stdlib.h>
#include "board.h"

void generate_puzzle(Board *puzzle, Board *solution) {
    FILE *f = fopen("sudoku.csv", "r");
    if (!f) {
        fprintf(stderr, "Could not open sudoku.csv\n");
        exit(1);
    }

    char line[256];

    // Skip header
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "Empty sudoku.csv\n");
        exit(1);
    }

    // For now: just take the first actual puzzle
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "No puzzles in sudoku.csv\n");
        exit(1);
    }

    char *quiz = strtok(line, ",");
    char *sol  = strtok(NULL, ",\n\r");

    if (!quiz || !sol || strlen(quiz) < 81 || strlen(sol) < 81) {
        fprintf(stderr, "Malformed line in sudoku.csv\n");
        exit(1);
    }

    // Fill puzzle and solution boards
    for (int i = 0; i < 81; ++i) {
        int r = i / 9;
        int c = i % 9;
        puzzle->cells[r][c]   = quiz[i] - '0';
        solution->cells[r][c] = sol[i] - '0';
    }

    fclose(f);
}


