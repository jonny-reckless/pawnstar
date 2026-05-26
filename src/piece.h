#pragma once
/// @file Chess piece types.
#include <stdint.h>

/// @brief Chess piece type. Values 1–6 match the bit encoding in move_t.

typedef uint8_t piece_t;

#define NONE   0 ///< No piece / empty square.
#define PAWN   1
#define KNIGHT 2
#define BISHOP 3
#define ROOK   4
#define QUEEN  5
#define KING   6
