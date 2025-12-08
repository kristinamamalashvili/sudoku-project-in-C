# sudoku-project-in-C

Multiplayer Sudoku
A two-player Sudoku game built in C using a client–server architecture. One program instance runs as the server, and two players connect as clients to compete on the same puzzle in real time. Works on Windows (Winsock) and Linux/macOS (POSIX sockets).

Features:
-Turn-based two-player Sudoku
-Server generates and solves puzzles
-Clients receive “YOUR_MOVE” and submit moves (ex: A7 4)
-Live scoreboard updated every turn
-Replay / Next Puzzle / Quit menu controlled by Player 1
-Cross-platform networking
-Clean modular structure (Sudoku logic separate from networking)
