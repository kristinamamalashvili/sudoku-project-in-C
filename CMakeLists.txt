cmake_minimum_required(VERSION 4.0)
project(sudoku_project_in_C C)

set(CMAKE_C_STANDARD 11)

add_executable(sudoku
        sudoku.c #this contains main()
        board.c
        solver.c
        generator.c
        net.c}

target_include_directories(sudoku PRIVATE   ${CMAKE_CURRENT_SOURCE_DIR} )