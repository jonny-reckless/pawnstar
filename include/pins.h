#pragma once
/// @file Pinned piece detection and allowed-move masks.
#include "bitboard.h"
#include <stdbool.h>

/// Forward declaration — full definition in position.h.
typedef struct position_t position_t;

/// @brief Tracks pinned pieces and their allowed destination squares.
typedef struct
{
    bitboard_t pinned_pieces;       ///< Squares of pinned friendly pieces.
    bitboard_t allowed_squares[64]; ///< Allowed destination squares per source square.
} pins_t;

/// @brief Compute pins for the side to move in the given position.
void pins_compute(pins_t *self, const position_t *position);

/// @brief Allowed destination squares for a piece on square s.
static inline bitboard_t pins_allowed_squares(const pins_t *self, square_t s)
{
    if (!self->pinned_pieces)
    {
        return ALL_SQUARES;
    }
    return (bitboard_from_square(s) & self->pinned_pieces) ? self->allowed_squares[s] : ALL_SQUARES;
}

/// @brief Is moving from->to legal given pin constraints?
static inline bool pins_is_allowed(const pins_t *self, square_t from, square_t to)
{
    if (!self->pinned_pieces)
    {
        return true;
    }
    return pins_allowed_squares(self, from) & bitboard_from_square(to);
}
