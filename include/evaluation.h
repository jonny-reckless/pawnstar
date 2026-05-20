#pragma once
/// @file Data and functions to evaluate a chess position.
#include "bitboard.h"
#include <stdbool.h>

typedef struct game_t game_t;

/// @brief Pawn structure for one color.
typedef struct
{
    bitboard_t passed_pawns;   ///< A passed pawn has no enemy pawns which can prevent it promoting.
    bitboard_t isolated_pawns; ///< An isolated pawn has no friendly pawns on either adjacent file.
    bitboard_t backward_pawns; ///< A backward pawn has no pawns to support it.
    bitboard_t doubled_pawns;  ///< A doubled pawn has a friendly pawn in front of it.
    bitboard_t defended_pawns; ///< A defended pawn has a friendly pawn defending it.
} pawn_structure_t;

/// @brief Evaluate the current game position. Returns a score in centipawns from the side-to-move's
/// perspective. Uses material, piece-square tables, pawn structure, mobility, and king safety.
/// @p alpha and @p beta are used for lazy evaluation (returns early if clearly outside the window).
int evaluate_position(const game_t *game, int alpha, int beta);
