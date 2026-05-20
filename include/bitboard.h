#pragma once
/// @file Types, values and functions for chess bitboards.
#include "square.h"
#include <stddef.h>
#include <stdint.h>

/// @brief A 64-bit set of squares. Bit 0 → a1 (LSB), bit 63 → h8 (MSB). LERF mapping.
typedef uint64_t bitboard_t;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/// @brief Return a bitboard with only square @p s set.
static inline bitboard_t bitboard_from_square(square_t s)
{
    return 1ULL << s;
}

/// @brief Return a bitboard with only the square at (file, rank) set.
static inline bitboard_t bitboard_from_coords(int file, int rank)
{
    return 1ULL << (file + 8 * rank);
}

// ---------------------------------------------------------------------------
// Bit queries
// ---------------------------------------------------------------------------

/// @brief Index of the least-significant set bit. Undefined behaviour if @p b is 0.
static inline square_t lsb(bitboard_t b)
{
    return (square_t)__builtin_ctzll(b);
}

/// @brief Index of the most-significant set bit. Undefined behaviour if @p b is 0.
static inline square_t msb(bitboard_t b)
{
    return (square_t)(63 - __builtin_clzll(b));
}

/// @brief Number of set bits (population count).
static inline int popcount(bitboard_t b)
{
    return __builtin_popcountll(b);
}

/// @brief Return @p b with all bits cleared except the least-significant one.
static inline bitboard_t isolate_lsb(bitboard_t b)
{
    return b & (0ULL - b);
}

/// @brief Return @p b with the least-significant bit cleared.
static inline bitboard_t clear_lsb(bitboard_t b)
{
    return b & (b - 1);
}

// ---------------------------------------------------------------------------
// Directional shift helpers (edge-safe)
// ---------------------------------------------------------------------------

static const uint64_t BB_NOT_FILE_A = 0xFEFEFEFEFEFEFEFEull; ///< All squares except file A; prevents west-shift wrap.
static const uint64_t BB_NOT_FILE_H = 0x7F7F7F7F7F7F7F7Full; ///< All squares except file H; prevents east-shift wrap.

/// @brief Shift all squares one step north (towards rank 8).
static inline bitboard_t bitboard_shift_north(bitboard_t b)
{
    return b << 8;
}
/// @brief Shift all squares one step north-east, masking the h-file to prevent wrap.
static inline bitboard_t bitboard_shift_northeast(bitboard_t b)
{
    return (b & BB_NOT_FILE_H) << 9;
}
/// @brief Shift all squares one step east, masking the h-file to prevent wrap.
static inline bitboard_t bitboard_shift_east(bitboard_t b)
{
    return (b & BB_NOT_FILE_H) << 1;
}
/// @brief Shift all squares one step south-east, masking the h-file to prevent wrap.
static inline bitboard_t bitboard_shift_southeast(bitboard_t b)
{
    return (b & BB_NOT_FILE_H) >> 7;
}
/// @brief Shift all squares one step south (towards rank 1).
static inline bitboard_t bitboard_shift_south(bitboard_t b)
{
    return b >> 8;
}
/// @brief Shift all squares one step south-west, masking the a-file to prevent wrap.
static inline bitboard_t bitboard_shift_southwest(bitboard_t b)
{
    return (b & BB_NOT_FILE_A) >> 9;
}
/// @brief Shift all squares one step west, masking the a-file to prevent wrap.
static inline bitboard_t bitboard_shift_west(bitboard_t b)
{
    return (b & BB_NOT_FILE_A) >> 1;
}
/// @brief Shift all squares one step north-west, masking the a-file to prevent wrap.
static inline bitboard_t bitboard_shift_northwest(bitboard_t b)
{
    return (b & BB_NOT_FILE_A) << 7;
}

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------

/// @brief Write an 8×8 ASCII board representation of @p b into @p buf (at least 200 bytes).
void bitboard_to_string(bitboard_t b, char *buf, size_t buf_size);

// ---------------------------------------------------------------------------
// Iteration macro
// ---------------------------------------------------------------------------

/// @brief Iterate over each set bit in @p board_expr from LSB to MSB, assigning
/// each square index to @p sq. @p sq must be pre-declared as a @c square_t variable.
#define BB_FOREACH(sq, board_expr)                                                                      \
    for (uint64_t _bb_##sq = (board_expr); _bb_##sq && ((sq) = (square_t)__builtin_ctzll(_bb_##sq), 1); \
         _bb_##sq &= _bb_##sq - 1)

// ---------------------------------------------------------------------------
// Common bitboard constants
// ---------------------------------------------------------------------------

// clang-format off
static const bitboard_t NO_SQUARES    = 0x0000000000000000ull; ///< Empty board.
static const bitboard_t ALL_SQUARES   = 0xFFFFFFFFFFFFFFFFull; ///< All 64 squares.
static const bitboard_t RANK1        = 0x00000000000000FFull; ///< Squares a1–h1.
static const bitboard_t RANK2        = 0x000000000000FF00ull; ///< Squares a2–h2.
static const bitboard_t RANK3        = 0x0000000000FF0000ull; ///< Squares a3–h3.
static const bitboard_t RANK4        = 0x00000000FF000000ull; ///< Squares a4–h4.
static const bitboard_t RANK5        = 0x000000FF00000000ull; ///< Squares a5–h5.
static const bitboard_t RANK6        = 0x0000FF0000000000ull; ///< Squares a6–h6.
static const bitboard_t RANK7        = 0x00FF000000000000ull; ///< Squares a7–h7.
static const bitboard_t RANK8        = 0xFF00000000000000ull; ///< Squares a8–h8.
static const bitboard_t FILE_A        = 0x0101010101010101ull; ///< Squares a1–a8.
static const bitboard_t FILE_B        = 0x0202020202020202ull; ///< Squares b1–b8.
static const bitboard_t FILE_C        = 0x0404040404040404ull; ///< Squares c1–c8.
static const bitboard_t FILE_D        = 0x0808080808080808ull; ///< Squares d1–d8.
static const bitboard_t FILE_E        = 0x1010101010101010ull; ///< Squares e1–e8.
static const bitboard_t FILE_F        = 0x2020202020202020ull; ///< Squares f1–f8.
static const bitboard_t FILE_G        = 0x4040404040404040ull; ///< Squares g1–g8.
static const bitboard_t FILE_H        = 0x8080808080808080ull; ///< Squares h1–h8.
static const bitboard_t WHITE_SQUARES = 0x55AA55AA55AA55AAull; ///< All light squares.
static const bitboard_t BLACK_SQUARES = 0xAA55AA55AA55AA55ull; ///< All dark squares.
// clang-format on
