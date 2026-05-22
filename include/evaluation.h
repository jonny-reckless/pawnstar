#pragma once
/// @file Data and functions to evaluate a chess position.
#include "bitboard.h"
#include <stdbool.h>

typedef struct game game_t;

/// @brief Evaluate the current game position. Returns a score in centipawns from the side-to-move's
/// perspective. Uses material, piece-square tables, pawn structure, mobility, and king safety.
/// @p alpha and @p beta are used for lazy evaluation (returns early if clearly outside the window).
int evaluate_position(const game_t *game, int alpha, int beta);
