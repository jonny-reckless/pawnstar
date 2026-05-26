#pragma once
/// @file Data and functions to evaluate a chess position.
#include "bitboard.h"
#include "color.h"
#include "piece.h"
#include "position.h"

#include <stdbool.h>

/// @brief Full static evaluation of @p position.
/// Returns a score in centipawns from the side-to-move's perspective (positive = better for the
/// side to move).  Evaluation is layered:
///   1. Lazy cutoff: if the incremental material+PST score (position_t::scores) already lies
///      outside [alpha−200, beta+200], that score is returned immediately.
///   2. Bishop-pair bonus (+30 cp per side with both bishops).
///   3. Pawn structure: passed pawns (rank-scaled bonus), defended (+5 cp each), backward
///      (−10 cp), doubled (−10 cp), isolated (−20 cp).
///   4. Mobility: sliding-piece attack counts mapped through BISHOP_ATTACK_SCORES /
///      ROOK_ATTACK_SCORES look-up tables.
///   5. King safety: pawn-shelter score (three ranks deep, ±15/10/5 cp per pawn), open-file
///      penalty (−15/10 cp), adjacent-piece bonus (+10/5 cp), and PST selection based on endgame
///      detection (KING_SQUARE_MIDGAME vs KING_SQUARE_ENDGAME).
/// @param position  Position to evaluate.
/// @param alpha     Lower search bound used for lazy cutoff.
/// @param beta      Upper search bound used for lazy cutoff.
/// @return Centipawn score from the side-to-move's perspective.
int evaluate_position(const position_t *position, int alpha, int beta);

/// @brief Return the combined material value + piece-square bonus for @p piece on @p square.
/// Values: pawn=100, knight=300, bishop=300, rook=500, queen=900, king=0.
/// The square table is mirrored vertically for Black (RANK_FLIP XOR).
/// Used during position setup to seed the incremental position_t::scores[2] fields.
/// @param piece   Piece type.
/// @param color   Piece color (determines rank-flip orientation).
/// @param square  LERF square index (0=a1, 63=h8) from White's perspective.
/// @return Material + PST score in centipawns.
int eval_piece_square_score(piece_t piece, color_t color, square_t square);

/// @brief True when the position is in the endgame phase.
/// Endgame is defined as: for every color that still has a queen, that side has no rooks and
/// at most one minor piece.  Positions with no queens are always considered endgame.
/// Used to switch king PST and to suppress null-move pruning.
static inline bool is_in_endgame(const position_t *pos)
{
    for (color_t color = WHITE; color <= BLACK; ++color)
    {
        const bitboard_t friendly = position_pieces_of_color(pos, color);
        if ((pos->queens & friendly) == 0)
        {
            continue;
        }
        if (popcount(pos->rooks & friendly) > 0 || popcount((pos->knights | pos->bishops) & friendly) > 1)
        {
            return false;
        }
    }
    return true;
}
