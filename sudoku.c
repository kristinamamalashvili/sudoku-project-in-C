#include "sudoku.h"
#include "board.h"
#include "generator.h"
#include "solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>


static void copy_board(Board dst, const Board src)
{
    for (int r = 0; r < SIZE; r++) {
        for (int c = 0; c < SIZE; c++) {
            dst[r][c] = src[r][c];
        }
    }
}

typedef enum {
    MOVE_OK,
    MOVE_OUT_OF_RANGE,
    MOVE_FIXED_CELL,
    MOVE_ALREADY_FILLED,
    MOVE_BREAKS_RULES
} MoveStatus;

// Check if a move is legal (but not whether it matches solution).
static MoveStatus validate_move(const Board puzzle,
                                const Board current,
                                int row, int col, int value)
{
    if (row < 0 || row >= SIZE || col < 0 || col >= SIZE ||
        value < 1 || value > 9) {
        return MOVE_OUT_OF_RANGE;
    }

    // cannot change original puzzle clues
    if (puzzle[row][col] != 0)
        return MOVE_FIXED_CELL;

    // cannot rewrite already filled cell
    if (current[row][col] != 0)
        return MOVE_ALREADY_FILLED;

    // Sudoku rules: row/col/3x3
    if (!board_is_move_valid(current, row, col, value))
        return MOVE_BREAKS_RULES;

    return MOVE_OK;
}


// Parse input like "A7 4" or "a7 4" into row/col/value.
// Returns true on success, false otherwise.
static bool parse_A7_move(const char *line, int *row, int *col, int *value)
{
    char rowChar;
    int c, v;

    if (sscanf(line, " %c%d %d", &rowChar, &c, &v) != 3) {
        return false;
    }

    if (rowChar >= 'a' && rowChar <= 'z')
        rowChar = (char)(rowChar - 'a' + 'A');

    if (rowChar < 'A' || rowChar > 'I') return false;
    if (c < 1 || c > 9) return false;
    if (v < 1 || v > 9) return false;

    *row   = rowChar - 'A';  // 0..8
    *col   = c - 1;          // 0..8
    *value = v;              // 1..9

    return true;
}

// Read a move with a soft time limit:
//   - We measure how long the user *takes* to type and press Enter,
//     but we don't actually stop them mid-typing.
// Returns:
//   1  = got a valid move in time (row/col/value set)
//   0  = input took too long (time up)
//  -1  = EOF / input error
//  -2  = bad format (not A7 4)
static int read_move_A7_with_timer(int *row, int *col, int *value, int seconds)
{
    char line[128];

    time_t start = time(NULL);

    printf("You have %d seconds. Enter move like A7 4: ", seconds);
    fflush(stdout);

    if (!fgets(line, sizeof(line), stdin)) {
        return -1;  // EOF / error
    }

    time_t end = time(NULL);
    if ((int)(end - start) > seconds) {
        return 0;   // took too long
    }

    if (!parse_A7_move(line, row, col, value)) {
        return -2;  // bad format
    }

    return 1;
}

ProgramMode parse_mode(int argc, char *argv[], int *out_player_id)
{
    if (argc < 2) {
        fprintf(stderr,
                "Usage:\n"
                "  %s server\n"
                "  %s client 1\n"
                "  %s client 2\n",
                argv[0], argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "server") == 0) {
        *out_player_id = 0;
        return MODE_SERVER;
    }

    if (strcmp(argv[1], "client") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Error: client mode requires player number (1 or 2).\n");
            exit(EXIT_FAILURE);
        }

        int id = atoi(argv[2]);
        if (id != 1 && id != 2) {
            fprintf(stderr, "Error: invalid player id %d (must be 1 or 2).\n", id);
            exit(EXIT_FAILURE);
        }

        *out_player_id = id;
        return MODE_CLIENT;
    }

    fprintf(stderr, "Error: unknown mode '%s'. Use 'server' or 'client'.\n",
            argv[1]);
    exit(EXIT_FAILURE);
}

// ----------------- Server: actual game -----------------

int run_server(void)
{
    Board puzzle;
    Board solution;
    Board current;

    generate_puzzle(puzzle, solution);
    copy_board(current, puzzle);

    // Optional: check solver works
    Board check;
    copy_board(check, puzzle);
    if (!solve(check)) {
        fprintf(stderr, "Internal error: solver could not solve puzzle.\n");
        return 1;
    }

    struct {
        const char *name;
        int score;
    } players[2] = {
        { "Player 1", 0 },
        { "Player 2", 0 }
    };

    int turn = 0;              // 0 = P1, 1 = P2
    int seconds_per_turn = 20;

    printf("Two-player Sudoku.\n");
    printf("Input format: A7 4  (row A–I, column 1–9, value 1–9)\n");
    printf("Each turn: %d seconds to enter ONE move.\n", seconds_per_turn);
    printf("Correct number = +1 point. Wrong number = no change, turn passes.\n");
    printf("No input / too slow = turn lost.\n");

    while (!board_is_full(current)) {
        printf("\n==== %s's TURN ====\n", players[turn].name);
        board_print(current, stdout);

        int r, c, v;
        int res = read_move_A7_with_timer(&r, &c, &v, seconds_per_turn);

        if (res == 0) {
            printf("Time up! No move registered. Turn lost.\n");
            turn = 1 - turn;
            continue;
        }
        if (res == -1) {
            printf("Input error. Ending game.\n");
            break;
        }
        if (res == -2) {
            printf("Invalid input. Use format like A7 4.\n");
            turn = 1 - turn;
            continue;
        }

        MoveStatus status = validate_move(puzzle, current, r, c, v);

        if (status != MOVE_OK) {
            switch (status) {
                case MOVE_OUT_OF_RANGE:
                    printf("Move out of range.\n");
                    break;
                case MOVE_FIXED_CELL:
                    printf("Cannot change an original puzzle clue.\n");
                    break;
                case MOVE_ALREADY_FILLED:
                    printf("That cell is already filled.\n");
                    break;
                case MOVE_BREAKS_RULES:
                    printf("That move breaks Sudoku rules.\n");
                    break;
                default:
                    printf("Unknown move error.\n");
                    break;
            }
            turn = 1 - turn;
            continue;
        }

        // Check against solution
        if (solution[r][c] == v) {
            current[r][c] = v;
            players[turn].score++;
            printf("Correct! %s gains a point.\n", players[turn].name);
        } else {
            printf("Wrong number. Board stays the same.\n");
        }

        turn = 1 - turn;
    }

    printf("\n=== GAME OVER ===\n");
    board_print(current, stdout);
    printf("Final Scores:\n");
    printf("Player 1: %d\n", players[0].score);
    printf("Player 2: %d\n", players[1].score);

    if (players[0].score > players[1].score)
        printf("Winner: Player 1!\n");
    else if (players[1].score > players[0].score)
        printf("Winner: Player 2!\n");
    else
        printf("It's a tie!\n");

    return 0;
}


int run_client(int player_id)
{
    printf("CLIENT MODE: Player %d\n", player_id);
    printf("(Client functionality not implemented — placeholder.)\n");
    return 0;
}


int main(int argc, char *argv[])
{
    int player_id = 0;
    ProgramMode mode = parse_mode(argc, argv, &player_id);

    if (mode == MODE_SERVER)
        return run_server();
    else
        return run_client(player_id);
}
