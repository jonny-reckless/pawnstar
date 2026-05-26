#pragma once
/// @file Data and functions to evaluate a chess position.
#include "bitboard.h"
#include "color.h"
#include "piece.h"
#include "position.h"

#include <stdbool.h>

/// @brief Evaluate @p position. Returns a score in centipawns from the side-to-move's perspective.
/// Uses material, piece-square tables, pawn structure, mobility, and king safety.
/// @p alpha and @p beta are used for lazy evaluation (returns early if clearly outside the window).
int evaluate_position(const position_t *position, int alpha, int beta);

int eval_piece_square_score(piece_t piece, color_t color, square_t square);
