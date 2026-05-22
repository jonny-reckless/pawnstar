cmake_minimum_required(VERSION 3.14)
project(pawnstar CXX C)

set(CMAKE_C_STANDARD   17)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED   ON)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ─── Google Test ─────────────────────────────────────────────────────────────
include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.tar.gz
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# ─── Generated data (precomputed bitboard tables + Zobrist hashes) ────────────
set(GENERATOR_SRC  ${CMAKE_SOURCE_DIR}/generate_constants/generate_constants.c)
set(GENERATOR_EXE  ${CMAKE_BINARY_DIR}/gen_constants)
set(GENERATED_DATA ${CMAKE_SOURCE_DIR}/src/generated_data.c)

add_custom_command(
    OUTPUT  ${GENERATED_DATA}
    COMMAND ${CMAKE_C_COMPILER} -std=c17 -mbmi2 -o ${GENERATOR_EXE} ${GENERATOR_SRC}
    COMMAND ${GENERATOR_EXE} > ${GENERATED_DATA}
    DEPENDS ${GENERATOR_SRC}
    COMMENT "Generating precomputed bitboard data"
)
add_custom_target(generated_data DEPENDS ${GENERATED_DATA})

# ─── Engine static library ───────────────────────────────────────────────────
# Excludes main.c (entry point) and tests_*.c (embedded UCI test runners).
add_library(engine STATIC
    src/chess_clock.c
    src/debug_hashtable.c
    src/evaluation.c
    src/game.c
    ${GENERATED_DATA}
    src/history_table.c
    src/move.c
    src/opening_book.c
    src/pins.c
    src/position.c
    src/search_alphabeta.c
    src/search_quiescent.c
    src/search_root_node.c
    src/static_exchange_evaluation.c
    src/transposition_table.c
)
add_dependencies(engine generated_data)
target_include_directories(engine PUBLIC include)
target_compile_options(engine PRIVATE -mbmi2 -O3 -Wall -Wextra)
target_compile_definitions(engine PRIVATE
    DEBUGX=1
    _POSIX_C_SOURCE=200809L
    NDEBUG
)

# ─── Test executables ────────────────────────────────────────────────────────
enable_testing()

function(add_engine_test name)
    add_executable(${name} ${ARGN})
    target_link_libraries(${name} PRIVATE engine gtest_main)
    target_compile_options(${name} PRIVATE -mbmi2)
    target_compile_definitions(${name} PRIVATE
        DEBUGX=0
        _POSIX_C_SOURCE=200809L
        NDEBUG
    )
    add_test(NAME ${name} COMMAND ${name})
endfunction()

add_engine_test(test_perft         tests/test_perft.cpp tests/perft_results.c)
add_engine_test(test_bratko_kopec  tests/test_bratko_kopec.cpp)
add_engine_test(test_see           tests/test_see.cpp)
