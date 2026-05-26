#pragma once
/// @file game_t: top-level game state owning all search infrastructure.

#include <semaphore.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <threads.h>

#include "chess_clock.h"
#include "constants.h"
#include "history_table.h"
#include "opening_book.h"
#include "position.h"
#include "transposition_table.h"

#define POSITION_STACK_SIZE 256 ///< Maximum number of positions in the game history + search stack.

/// @brief All state required to play and search a game of chess.
typedef struct game
{
    transposition_table_t transposition_table;            ///< Main transposition table (64 MB default).
    history_table_t       history_table;                  ///< History heuristic counts.
    chess_clock_t         time_control;                   ///< Clock and time-control parameters.
    opening_book_t        book;                           ///< Opening book.
    int                   node_count;                     ///< Nodes visited during the current search.
    atomic_bool           is_cancel_pending;              ///< Set by the UCI stop command; causes search to terminate.
    thrd_t                worker_thread;                  ///< Background thread running the search.
    bool                  worker_running;                 ///< True while worker_thread is alive and joinable.
    sem_t                 parallel_slots;                 ///< Semaphore controlling max parallel workers (capacity = NumCPU).
    position_t            positions[POSITION_STACK_SIZE]; ///< Position history stack (copy-make).
    position_t           *position;                       ///< Pointer to the current position (top of stack).
} game_t;

/// @brief Allocate and initialize all sub-components (transposition tables, history, etc.).
void game_init(game_t *self);

/// @brief Free all heap memory owned by the game.
void game_free(game_t *self);

/// @brief Reset game state and set up the position described by @p fen_string.
void game_new_game(game_t *self, const char *fen_string);

/// @brief Reset game state and set up the standard starting position.
void game_new_game_default(game_t *self);

/// @brief Parse @p move_str (UCI long-algebraic notation) and play the move.
/// Returns the move played, or move_none() if the string is invalid.
move_t game_play_move_from_string(game_t *self, const char *move_str);

/// @brief Push a new position onto the stack and apply @p move to it.
void game_play_move(game_t *self, move_t move);

/// @brief Push a new position onto the stack and apply a null move (pass the turn) to it.
void game_make_null_move(game_t *self);

/// @brief Pop the top position off the stack (undo the last move or null move).
void game_undo_move(game_t *self);

/// @brief Start the search worker thread (non-blocking; result delivered via UCI @c bestmove output).
void game_start_thinking(game_t *self);

/// @brief Signal the worker thread to stop and wait for it to finish.
void game_stop_thinking(game_t *self);

/// @brief True if the current position is checkmate or stalemate.
bool game_is_game_over(game_t *self);

/// @brief True if the current position has been reached three times (draw by repetition).
bool game_is_draw_by_repetition(const game_t *self);

/// @brief True if the fifty-move rule applies (100 half-moves without a pawn move or capture).
bool game_is_draw_by_fifty_moves(const game_t *self);

/// @brief Assign sort scores to all moves in @p moves and sort them descending.
/// Scores combine MVV-LVA (captured vs moving piece values) and history heuristic counts.
void game_score_and_sort_moves(game_t *self, move_list_t *moves, int ply);
