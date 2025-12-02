#include "generator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "board.h"

void generate_puzzle(Board puzzle, Board solution) {
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

    // Count how many puzzle lines there are (excluding header)
    long count = 0;
    long pos_after_header = ftell(f);  // remember after header
    while (fgets(line, sizeof(line), f)) {
        count++;
    }

    if (count == 0) {
        fprintf(stderr, "No puzzles found in sudoku.csv\n");
        exit(1);
    }

    // Pick a random puzzle line
    srand((unsigned) time(NULL));
    long target = rand() % count;

    // Go back to the line after the header
    fseek(f, pos_after_header, SEEK_SET);

    // Skip until we reach the chosen puzzle line
    for (long i = 0; i < target; i++) {
        if (!fgets(line, sizeof(line), f)) {
            fprintf(stderr, "Unexpected EOF in sudoku.csv\n");
            exit(1);
        }
    }

    // Read the selected puzzle line
    if (!fgets(line, sizeof(line), f)) {
        fprintf(stderr, "Failed to read selected puzzle\n");
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
        puzzle[r][c] = quiz[i] - '0';
        solution[r][c] = sol[i] - '0';
    }

    fclose(f);
}
