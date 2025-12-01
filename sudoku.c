#include "sudoku.h"
#include "board.h"
#include "generator.h"
#include "solver.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>

#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdarg.h>

static int client_socks[3] = {0,0,0};   // 1 = P1, 2 = P2

// server-side broadcast
static void broadcast(const char *text) {
    for (int i = 1; i <= 2; i++) {
        if (client_socks[i] > 0)
            send(client_socks[i], text, strlen(text), 0);
    }
}

static void broadcastf(const char *fmt, ...) {
    char buf[2048];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    broadcast(buf);
}

#define PRINTF(...) do {                     \
char __buf[2048];                        \
int __n = sprintf(__buf, __VA_ARGS__);   \
write(1, __buf, __n);                    \
broadcast(__buf);                        \
} while(0)

// Read one line (client)
static int recv_line(int sock, char *buf, int max) {
    int i = 0;
    while (i < max - 1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n <= 0)
            return 0;
        if (c == '\n')
            break;
        buf[i++] = c;
    }
    buf[i] = 0;
    return 1;
}

// New Function: Waits for input from a specific client socket
static int read_move_from_client(int client_sock, int *row, int *col, int *value, int seconds)
{
    // The server needs to send the prompt to the client FIRST
    // The client is designed to wait for "YOUR_MOVE"
    send(client_sock, "YOUR_MOVE", strlen("YOUR_MOVE"), 0);

    // Now use select() to wait on the socket with a timeout
    fd_set read_fds;
    struct timeval tv;

    FD_ZERO(&read_fds);
    FD_SET(client_sock, &read_fds);

    tv.tv_sec = seconds;
    tv.tv_usec = 0;

    int ready = select(client_sock + 1, &read_fds, NULL, NULL, &tv);

    if (ready == 0) {
        return 0; // Timeout
    }
    if (ready < 0) {
        return -1; // Error
    }

    // Check if the socket is ready to be read
    if (FD_ISSET(client_sock, &read_fds)) {
        char line[128];
        if (!recv_line(client_sock, line, sizeof(line))) {
            return -1; // Connection closed/error
        }

        // Use your existing parsing function
        if (!parse_A7_move(line, row, col, value)) {
            return -2; // Bad format
        }
        return 1; // Success
    }
    return -1; // Should not happen
}

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
                "  %s client [ID] [ADDRESS] [PORT]\n", // <-- UPDATED USAGE
                argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "server") == 0) {
        *out_player_id = 0;
        return MODE_SERVER;
    }

    if (strcmp(argv[1], "client") == 0) {
        // Must have ID, ADDRESS, and PORT (4 arguments total: 0, 1, 2, 3, 4)
        if (argc < 5) { // <-- CHECK FOR 5 ARGS
            fprintf(stderr, "Error: client mode requires player number (1 or 2), server address, and port.\n");
            exit(EXIT_FAILURE);
        }

        int id = atoi(argv[2]);
        if (id != 1 && id != 2) {
            fprintf(stderr, "Error: invalid player id %d (must be 1 or 2).\n", id);
            exit(EXIT_FAILURE);
        }

        // CAPTURE ARGUMENTS
        g_server_addr = argv[3]; // <-- Store Address
        g_server_port = atoi(argv[4]); // <-- Store Port

        if (g_server_port <= 0) {
            fprintf(stderr, "Error: invalid port number '%s'.\n", argv[4]);
            exit(EXIT_FAILURE);
        }

        *out_player_id = id;
        return MODE_CLIENT;
    }

    fprintf(stderr, "Error: unknown mode '%s'. Use 'server' or 'client'.\n",
            argv[1]);
    exit(EXIT_FAILURE);
}
int run_server(void)
{
    int seconds_per_turn = 20;
    int port = 5555;

    PRINTF("SERVER: Waiting for two clients on port %d...\n", port);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    int yes = 1;
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 2);

    client_socks[1] = accept(s, NULL, NULL);
    PRINTF("PLAYER 1 connected.\n");

    client_socks[2] = accept(s, NULL, NULL);
    PRINTF("PLAYER 2 connected.\n");

    printf("Two-player Sudoku.\n");
    printf("Input format: A7 4  (row A–I, column 1–9, value 1–9)\n");
    printf("Each turn: %d seconds to enter ONE move.\n", seconds_per_turn);
    printf("Correct number = +1 point. Wrong number = no change, turn passes.\n");
    printf("No input / too slow = turn lost.\n");

    // Outer loop: each iteration is a NEW puzzle ("next exercise")
    while (1) {
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

        bool next_puzzle = false;  // control flag for inner loop

        // Inner loop: allows replaying the SAME puzzle
        while (!next_puzzle) {
            struct {
                const char *name;
                int score;
            } players[2] = {
                { "Player 1", 0 },
                { "Player 2", 0 }
            };

            int turn = 0;  // 0 = P1, 1 = P2

            // reset board (same exercise) each time we replay
            copy_board(current, puzzle);

            // ---- play this puzzle once ----
            while (!board_is_full(current)) {
                int player_index = turn + 1;
                int turn_sock = client_socks[player_index];
                printf("\n==== %s's TURN ====\n", players[turn].name);
                board_print(current, stdout);

                int r, c, v;
                int res = read_move_from_client(turn_sock, &r, &c, &v, seconds_per_turn);

                if (res == 0) {
                    printf("Time up! No move registered. Turn lost.\n");
                    turn = 1 - turn;
                    continue;
                }
                if (res == -1) {
                    printf("Input error. Ending game.\n");
                    return 0;
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

            // ---- puzzle finished once ----
            printf("\n=== EXERCISE COMPLETE ===\n");
            board_print(current, stdout);
            printf("Scores for this exercise:\n");
            printf("Player 1: %d\n", players[0].score);
            printf("Player 2: %d\n", players[1].score);

            if (players[0].score > players[1].score)
                printf("Winner: Player 1!\n");
            else if (players[1].score > players[0].score)
                printf("Winner: Player 2!\n");
            else
                printf("It's a tie!\n");

            // ---- ask what to do next for this exercise ----
            char buf[32];
            char choice;

            while (1) {
                printf("\nWhat do you want to do now?\n");
                printf("  R - Replay the SAME exercise\n");
                printf("  N - Next exercise (new puzzle)\n");
                printf("  Q - Quit\n");
                printf("Choice: ");
                fflush(stdout);

                if (!fgets(buf, sizeof(buf), stdin)) {
                    // EOF or input error -> just quit
                    return 0;
                }

                if (sscanf(buf, " %c", &choice) != 1) {
                    printf("Invalid input.\n");
                    continue;
                }

                choice = (char)toupper((unsigned char)choice);

                if (choice == 'R') {
                    printf("\nReplaying the same exercise...\n");
                    // break only the choice loop, replay same puzzle
                    break;
                } else if (choice == 'N') {
                    printf("\nLoading next exercise...\n");
                    next_puzzle = true;
                    break;
                } else if (choice == 'Q') {
                    printf("\nQuitting the game. Bye!\n");
                    return 0;
                } else {
                    printf("Please enter R, N, or Q.\n");
                }
            }
            // if choice == 'R', inner while(!next_puzzle) repeats with same puzzle
        }
        // if next_puzzle == true, outer while(1) continues and generates new puzzle
    }
}


int run_client(int player_id, const char *server_addr, int port)
{
    printf("CLIENT %d connecting...\n", player_id);

    int s = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_addr, &addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        return 1;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        return 1;
    }

    printf("Connected to server.\n");

    char buf[512];

    while (1) {
        if (!recv_line(s, buf, sizeof(buf)))
            break;

        if (strcmp(buf, "YOUR_MOVE") == 0) {
            printf("Enter move (A7 4): ");
            fflush(stdout);

            char input[128];
            fgets(input, sizeof(input), stdin);
            input[strcspn(input, "\n")] = 0;

            send(s, input, strlen(input), 0);
            send(s, "\n", 1, 0);
        } else {
            printf("%s\n", buf);
        }
    }

    close(s);
    return 0;

}


int main(int argc, char *argv[])
{
        int player_id = 0;
        ProgramMode mode = parse_mode(argc, argv, &player_id);

        if (mode == MODE_SERVER)
            return run_server();
        else
            // PASS THE CAPTURED ARGUMENTS to run_client
                return run_client(player_id, g_server_addr, g_server_port);
}