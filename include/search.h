/// @file Search entry points: alpha-beta, quiescence, and root-node search.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"

/// @brief Result of searching a single move at the root node.
typedef struct
{
    int  score;       ///< Alpha-beta score from the child position's perspective.
    bool is_checking; ///< True if the move gives check.
} single_move_result_t;

/// @brief Search from the root position and return the best move found.
/// Drives iterative deepening and outputs UCI @c info lines.
move_t search_root_node(game_t *game);

/// @brief Alpha-beta search. Returns the score for the side to move.
/// Uses the transposition table, null-move pruning, and late-move reduction.
int search(game_t *game, int depth, int ply, int alpha, int beta, variation_t *parent_pv);

/// @brief Play @p move, search the resulting position to @p depth, then undo the move.
single_move_result_t search_single_move(game_t *game, int depth, int ply, int alpha, int beta, move_t move,
                                        variation_t *pv, int move_index);

/// @brief Quiescence search: extends with captures only until a quiet position is reached.
int search_quiescent(game_t *game, int depth, int ply, int alpha, int beta);
