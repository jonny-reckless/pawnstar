/// @file Search entry points: alpha-beta, quiescence, and root-node search.
#pragma once

#include "constants.h"
#include "game.h"
#include "move.h"
#include "search_state.h"

/// @brief Search from the root position and return the best move found.
/// Checks the opening book first; returns immediately if only one legal move exists.
/// Drives iterative deepening from START_DEPTH up to MAX_PLY, emitting UCI @c info lines for
/// each depth at which the best move or score improves.  Early termination heuristics fire when
/// the best move is stable across all previous depths AND the score does not fluctuate beyond
/// SCORE_INSTABILITY_THRESHOLD.
/// @param game  Shared game state (time control, cancel flag, TT, etc.).
/// @return The best move found, or move_none() if the position has no legal moves.
move_t search_root_node(game_t *game);

/// @brief Negamax alpha-beta search with PVS, null-move pruning, and LMR.
/// All scores are from the perspective of the side to move (negamax convention).
/// The transposition table is consulted before move generation; a matching entry at sufficient
/// depth may return immediately (CUT/ALL nodes) or extend the search by one ply (PV nodes).
/// Null-move pruning (R=4) is attempted at null-window nodes when the side to move has a
/// material advantage and is not in the endgame.  LMR reduces quiet non-pawn moves at
/// null-window nodes after the 4th move (−1 ply), 7th move (−2 ply), and 7th move with zero
/// history count (−3 ply); reductions are re-searched at full depth if they raise alpha.
/// @param ss          Per-thread search state (position stack, hash stack, node counter).
/// @param depth       Remaining depth in plies; may become negative in quiescence.
/// @param ply         Distance from the root; used for mate-distance scores and TT indexing.
/// @param alpha       Lower bound: the score the side to move must beat to be considered.
/// @param beta        Upper bound: if score ≥ beta the parent will not choose this line.
/// @param parent_pv   Principal variation accumulator; updated when alpha is raised.
/// @return Score in centipawns from the side-to-move's perspective, or SEARCH_CANCELLED_SCORE.
int search(search_state_t *ss, int depth, int ply, int alpha, int beta, variation_t *parent_pv);

/// @brief Apply @p move to the current position, recurse into search(), then undo the move.
/// Applies PVS: at a PV node (beta > alpha+1), moves after the first are first searched with a
/// null window (−alpha−1, −alpha); only if that scout raises alpha is a full-window re-search done.
/// Promotion moves receive a +1 depth extension before the recursive call.
/// @param ss          Per-thread search state.
/// @param depth       Remaining depth passed to the child search.
/// @param ply         Current ply (incremented by 1 before the child call).
/// @param alpha       Current alpha at this node.
/// @param beta        Current beta at this node.
/// @param move        Move to play and search.
/// @param pv          PV accumulator for the child variation.
/// @param move_index  Zero-based index of this move in the ordered move list (0 = TT move).
/// @return Child position's score negated to the parent's perspective.
int search_single_move(search_state_t *ss, int depth, int ply, int alpha, int beta, move_t move, variation_t *pv,
                       int move_index);

/// @brief Quiescence search: evaluates captures until a quiet (non-capturing) position is reached.
/// The standing-pat heuristic returns the static evaluation if it already exceeds beta.
/// When the side to move is in check, falls back to a full search() call so all legal replies are
/// considered.  No null-move or LMR is applied; depth merely bounds recursion from the horizon.
/// @param ss    Per-thread search state.
/// @param depth Remaining depth (typically ≤ 0 on entry; decremented each capture ply).
/// @param ply   Current ply from root (bounds MAX_PLY cutoff and move scoring).
/// @param alpha Lower bound.
/// @param beta  Upper bound.
/// @return Score in centipawns from the side-to-move's perspective, or SEARCH_CANCELLED_SCORE.
int search_quiescent(search_state_t *ss, int depth, int ply, int alpha, int beta);
