/// @file Search entry points: alpha-beta, quiescence, and root-node search.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "search_state.h"

/// @brief Search from the root position and return the best move found.
/// Drives iterative deepening and outputs UCI @c info lines.
move_t search_root_node(game_t *game);

/// @brief Alpha-beta search. Returns the score for the side to move.
/// Uses the transposition table, null-move pruning, late-move reduction, and (at depth ≥ 4)
/// Young Brothers Wait parallel search.
int search(search_state_t *ss, int depth, int ply, int alpha, int beta, variation_t *parent_pv);

/// @brief Play @p move, search the resulting position to @p depth, then undo the move.
/// Returns the score from the child position's perspective.
int search_single_move(search_state_t *ss, int depth, int ply, int alpha, int beta, move_t move,
                       variation_t *pv, int move_index);

/// @brief Quiescence search: extends with captures only until a quiet position is reached.
int search_quiescent(search_state_t *ss, int depth, int ply, int alpha, int beta);
