#pragma once
/// @file Chess piece types.
#include <stdint.h>

/// @brief Chess piece type. Values 1–6 match the bit encoding in move_t.
typedef enum
{
    NONE   = 0, ///< No piece / empty square.
    PAWN   = 1,
    KNIGHT = 2,
    BISHOP = 3,
    ROOK   = 4,
    QUEEN  = 5,
    KING   = 6,
} piece_t;
