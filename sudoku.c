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
#include <stdarg.h>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #define close closesocket
    typedef int socklen_t;// Portability macro
#else
    #include <unistd.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <netinet/in.h>
#endif

static char *g_server_addr = NULL;
static int g_server_port = 0;

static int client_socks[3] = {0,0,0};   // 1 = P1, 2 = P2


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
    int __n = snprintf(__buf, sizeof(__buf), __VA_ARGS__); \
    (void)__n;                               \
    printf("%s", __buf);                     \
    broadcast(__buf);                        \
} while(0)


// Read one line (client -> server)
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

// Discard any pending data from this client's socket so they can't "pre-type" moves
static void flush_client_socket(int sock)
{
    char tmp[256];
    fd_set rfds;
    struct timeval tv;
    int r;

    for (;;) {
        FD_ZERO(&rfds);
        FD_SET(sock, &rfds);
        tv.tv_sec = 0;
        tv.tv_usec = 0;

        r = select(sock + 1, &rfds, NULL, NULL, &tv);
        if (r <= 0) {
            // No more data ready, or error
            break;
        }

        if (FD_ISSET(sock, &rfds)) {
            int n = recv(sock, tmp, sizeof(tmp), 0);
            if (n <= 0) {
                // Error or connection closed
                break;
            }
        }
    }
}


static bool parse_A7_move(const char *line, int *row, int *col, int *value)
{
    char rowChar;
    int c, v;

    // FORMAT: Letter (row), Number (column), Value
    // Example: "A7 4"
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

// Wait for a move from a specific client socket, with timeout.
// Returns:
//   1  = got a valid move in time (row/col/value set)
//   0  = timeout
//  -1  = connection closed / error
//  -2  = bad format
static int read_move_from_client(int client_sock, int *row, int *col, int *value, int seconds)
{
    flush_client_socket(client_sock);

    send(client_sock, "YOUR_MOVE\n", (int)strlen("YOUR_MOVE\n"), 0);

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

    if (FD_ISSET(client_sock, &read_fds)) {
        char line[128];
        if (!recv_line(client_sock, line, sizeof(line))) {
            return -1; // Connection closed/error
        }

        if (!parse_A7_move(line, row, col, value)) {
            return -2; // Bad format
        }
        return 1; // Success
    }

    return -1; // Should not happen
}

static void copy_board(Board dst, const Board src)
{
    for (int r = 0; r < BOARDSIZE; r++) {
        for (int c = 0; c < BOARDSIZE; c++) {
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
    if (row < 0 || row >= BOARDSIZE || col < 0 || col >= BOARDSIZE ||
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


ProgramMode parse_mode(int argc, char *argv[], int *out_player_id)
{
    if (argc < 2) {
        fprintf(stderr,
                "Usage:\n"
                "  %s server\n"
                "  %s client [ID] [ADDRESS] [PORT]\n",
                argv[0], argv[0]);
        exit(EXIT_FAILURE);
    }

    if (strcmp(argv[1], "server") == 0) {
        *out_player_id = 0;
        return MODE_SERVER;
    }

    if (strcmp(argv[1], "client") == 0) {
        if (argc < 5) {
            fprintf(stderr, "Error: client mode requires player number (1 or 2), server address, and port.\n");
            exit(EXIT_FAILURE);
        }

        int id = atoi(argv[2]);
        if (id != 1 && id != 2) {
            fprintf(stderr, "Error: invalid player id %d (must be 1 or 2).\n", id);
            exit(EXIT_FAILURE);
        }

        g_server_addr = argv[3];
        g_server_port = atoi(argv[4]);

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
    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (const char*)&yes, sizeof(yes));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(s, (struct sockaddr*)&addr, sizeof(addr));
    listen(s, 2);

    client_socks[1] = accept(s, NULL, NULL);
    send(client_socks[1], "YOU_ARE_PLAYER 1\n", 18, 0);
    PRINTF("PLAYER 1 connected.\n");

    client_socks[2] = accept(s, NULL, NULL);
    send(client_socks[2], "YOU_ARE_PLAYER 2\n", 18, 0);
    PRINTF("PLAYER 2 connected.\n");

    PRINTF("Two-player Sudoku.\n");
    PRINTF("Input format: A7 4 (row letter, column number, value).\n");
    PRINTF("Each turn: %d seconds to enter ONE move.\n", seconds_per_turn);
    PRINTF("Correct number = +1 point. Wrong number = no change, turn passes.\n");
    PRINTF("No input / too slow = turn lost.\n");

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

                // Announce whose turn it is to everyone
                PRINTF("\n==== %s's TURN ====\n", players[turn].name);

                // Show the board to EVERYONE (PRINTF broadcasts)
                char board_output_buffer[2048];
                board_to_string(current, board_output_buffer, sizeof(board_output_buffer));
                PRINTF("%s", board_output_buffer);
                // Debug: show board on server console too
                printf("\n[DEBUG] Current board on server:\n%s\n", board_output_buffer);
                fflush(stdout);


                int r, c, v;
                int res = read_move_from_client(turn_sock, &r, &c, &v, seconds_per_turn);

                if (res == 0) {
                    PRINTF("Time up! No move registered. Turn lost.\n");
                    turn = 1 - turn;
                    continue;
                }
                if (res == -1) {
                    // Fatal Error / Disconnection detected
                    PRINTF("\n*** %s disconnected. Ending game for all players. ***\n", players[turn].name);

                    // Close the socket that failed
                    if (turn_sock > 0) close(turn_sock);
                    client_socks[player_index] = 0;

                    PRINTF("SERVER SHUTDOWN initiated by player disconnection.\n");
                    exit(EXIT_SUCCESS);
                }
                if (res == -2) {
                    PRINTF("Invalid input. Use format like A7 4.\n");
                    turn = 1 - turn;
                    continue;
                }

                MoveStatus status = validate_move(puzzle, current, r, c, v);

                if (status != MOVE_OK) {
                    switch (status) {
                        case MOVE_OUT_OF_RANGE:
                            PRINTF("Move out of range.\n");
                            break;
                        case MOVE_FIXED_CELL:
                            PRINTF("Cannot change an original puzzle clue.\n");
                            break;
                        case MOVE_ALREADY_FILLED:
                            PRINTF("That cell is already filled.\n");
                            break;
                        case MOVE_BREAKS_RULES:
                            PRINTF("That move breaks Sudoku rules.\n");
                            break;
                        default:
                            PRINTF("Unknown move error.\n");
                            break;
                    }
                    turn = 1 - turn;
                    continue;
                }

                // Check against solution
                if (solution[r][c] == v) {
                    current[r][c] = v;
                    players[turn].score++;
                    PRINTF("Correct! %s gains a point.\n", players[turn].name);
                } else {
                    PRINTF("Wrong number. Board stays the same.\n");
                }

                turn = 1 - turn;
            }

            // ---- puzzle finished once ----
            PRINTF("\n=== EXERCISE COMPLETE ===\n");
            char final_board_output_buffer[2048];
            board_to_string(current, final_board_output_buffer, sizeof(final_board_output_buffer));
            PRINTF("%s", final_board_output_buffer);
            PRINTF("Scores for this exercise:\n");
            PRINTF("Player 1: %d\n", players[0].score);
            PRINTF("Player 2: %d\n", players[1].score);

            if (players[0].score > players[1].score)
                PRINTF("Winner: Player 1!\n");
            else if (players[1].score > players[0].score)
                PRINTF("Winner: Player 2!\n");
            else
                PRINTF("It's a tie!\n");

            // ---- ask what to do next for this exercise ----
            char buf[32];
            char choice;

            while (1) {
                PRINTF("\nWhat do you want to do now?\n");
                PRINTF("  R - Replay the SAME exercise\n");
                PRINTF("  N - Next exercise (new puzzle)\n");
                PRINTF("  Q - Quit\n");
                PRINTF("Choice: ");
                fflush(stdout);

                if (!fgets(buf, sizeof(buf), stdin)) {
                    // EOF or input error -> just quit
                    return 0;
                }

                if (sscanf(buf, " %c", &choice) != 1) {
                    PRINTF("Invalid input.\n");
                    continue;
                }

                choice = (char)toupper((unsigned char)choice);

                if (choice == 'R') {
                    PRINTF("\nReplaying the same exercise...\n");
                    break;          // replay same puzzle
                } else if (choice == 'N') {
                    PRINTF("\nLoading next exercise...\n");
                    next_puzzle = true;
                    break;          // go to new puzzle
                } else if (choice == 'Q') {
                    PRINTF("\nQuitting the game. Bye!\n");
                    return 0;
                } else {
                    PRINTF("Please enter R, N, or Q.\n");
                }
            }
        }
    }
}


int run_client(int player_id, const char *server_addr, int port)
{
    int my_player_id = 0;

    printf("CLIENT %d connecting...\n", player_id);

    int s = socket(AF_INET, SOCK_STREAM, 0);
    if (s < 0) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);

    if (inet_pton(AF_INET, server_addr, &addr.sin_addr) <= 0) {
        perror("Invalid address/Address not supported");
        close(s);
        return 1;
    }

    if (connect(s, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(s);
        return 1;
    }

    printf("Connected to server.\n");

    char recv_buf[4096];
    char input_buf[128];

    // Simple loop: always read from server; on YOUR_MOVE, read from stdin and send.
    while (1) {
        int n = recv(s, recv_buf, sizeof(recv_buf) - 1, 0);
        // --- Detect personal player identity message ---
        char *idmsg = strstr(recv_buf, "YOU_ARE_PLAYER");
        if (idmsg) {
            int id;
            if (sscanf(idmsg, "YOU_ARE_PLAYER %d", &id) == 1) {
                my_player_id = id;
                printf(">>> You are Player %d\n", my_player_id);
            }

            // Remove identity message from buffer before normal printing
            char *endline = strchr(idmsg, '\n');
            if (endline) endline++;
            if (endline) memmove(idmsg, endline, strlen(endline) + 1);
        }

        if (n <= 0) {
            printf("\nServer closed connection.\n");
            break;
        }
        recv_buf[n] = '\0';

        char *cursor = recv_buf;

        for (;;) {
            char *token = strstr(cursor, "YOUR_MOVE");
            if (!token) {
                // No more YOUR_MOVE in this chunk, just print the rest
                printf("%s", cursor);
                fflush(stdout);
                break;
            }

            // Print everything up to "YOUR_MOVE"
            *token = '\0';
            printf("%s", cursor);
            fflush(stdout);

            // Consume the token (and any trailing newline, if present)
            cursor = token + strlen("YOUR_MOVE");
            if (*cursor == '\n') cursor++;

            // Prompt user and send move
            printf("Enter move (A7 4): ");
            fflush(stdout);

            if (!fgets(input_buf, sizeof(input_buf), stdin)) {
                printf("\nInput closed. Exiting.\n");
                close(s);
                return 0;
            }
            input_buf[strcspn(input_buf, "\n")] = '\0';

            char send_buf[256];
            int len = snprintf(send_buf, sizeof(send_buf), "%s\n", input_buf);
            if (send(s, send_buf, len, 0) < 0) {
                perror("send");
                close(s);
                return 1;
            }
        }
    }

    close(s);
    return 0;
}



int main(int argc, char *argv[])
{
    int player_id = 0;
    ProgramMode mode = parse_mode(argc,argv, &player_id);

#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
#endif

    int result;
    if (mode == MODE_SERVER)
        result = run_server();
    else
        result = run_client(player_id, g_server_addr, g_server_port);

#ifdef _WIN32
    WSACleanup();
#endif

    return result;
}