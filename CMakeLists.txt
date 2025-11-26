cmake_minimum_required(VERSION 4.0)
project(untitled C)

set(CMAKE_C_STANDARD 11)

add_executable(
        src/sudoku.c #this contains main()
        src/board.c
        src/solver.c
        src/generator.c)

target_include_directories(sudoku PRIVATE src)