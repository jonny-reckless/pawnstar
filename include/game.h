#pragma once
/// @file game_t: top-level game state owning all search infrastructure.

#include <stdbool.h>
#include <threads.h>

#include "chess_clock.h"
#include "constants.h"
#include "history_table.h"
#include "opening_book.h"
#include "position.h"
#include "position_stack.h"
#include "transposition_table.h"

/// @brief All state required to play and search a game of chess.
typedef struct game_t
{
    transposition_table_t transposition_table; ///< Main transposition table (64 MB default).
    transposition_table_t quiescent_table;     ///< Separate transposition table for quiescence search.
    history_table_t       history_table;       ///< History heuristic counts.
    chess_clock_t         time_control;        ///< Clock and time-control parameters.
    opening_book_t        book;                ///< Opening book.
    int                   node_count;          ///< Nodes visited during the current search.
    bool                  is_cancel_pending;   ///< Set by the UCI stop command; causes the search to terminate.
    thrd_t                worker_thread;       ///< Background thread running the search.
    bool                  worker_running;      ///< True while worker_thread is alive and joinable.
    position_stack_t      positions;           ///< Full position history (used for repetition detection).
} game_t;

/// @brief Centipawn values for each piece type indexed by piece_t (NONE through KING).
static const int PIECE_VALUES[7] = {0, 100, 300, 300, 500, 900, 10000};

/// @brief Allocate and initialize all sub-components (transposition tables, history, etc.).
void game_init(game_t *self);

/// @brief Free all heap memory owned by the game.
void game_free(game_t *self);

/// @brief Mutable pointer to the current position (top of the history stack).
static inline position_t *game_current_position(game_t *self)
{
    return position_stack_back(&self->positions);
}

/// @brief Read-only pointer to the current position.
static inline const position_t *game_current_position_const(const game_t *self)
{
    return position_stack_back_const(&self->positions);
}

/// @brief Reset game state and set up the position described by @p fen_string.
void game_new_game(game_t *self, const char *fen_string);

/// @brief Reset game state and set up the standard starting position.
void game_new_game_default(game_t *self);

/// @brief Parse @p move_str (UCI long-algebraic notation) and play the move.
/// Returns the move played, or move_none() if the string is invalid.
move_t game_play_move_from_string(game_t *self, const char *move_str);

/// @brief Push @p move onto the position history stack (does not validate legality).
void game_play_move(game_t *self, move_t move);

/// @brief Push a null move (pass the turn; used in null-move pruning).
void game_make_null_move(game_t *self);

/// @brief Pop the most recent move from the position history stack.
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
