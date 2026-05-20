#pragma once
/// @file Sliding-piece attack lookup using BMI2 PEXT bitboards (with classical fallback).
#include "generated_data.h"
#include "piece.h"
#include <immintrin.h>

#ifndef USE_PEXT_BITBOARDS
#define USE_PEXT_BITBOARDS 1
#endif

#if USE_PEXT_BITBOARDS

#if __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Winvalid-constexpr"
#endif

/// @brief Bishop attacks from square @p s given the occupied squares.
static inline bitboard_t bishop_attacks(bitboard_t occupied_squares, square_t s)
{
    const pext_bitboard_t *p = &BISHOP_PEXTS[s];
    return p->attacks[p->indices[_pext_u64(occupied_squares, p->occupancy_mask)]];
}

/// @brief Rook attacks from square @p s given the occupied squares.
static inline bitboard_t rook_attacks(bitboard_t occupied_squares, square_t s)
{
    const pext_bitboard_t *p = &ROOK_PEXTS[s];
    return p->attacks[p->indices[_pext_u64(occupied_squares, p->occupancy_mask)]];
}

#if __GNUC__
#pragma GCC diagnostic pop
#endif

#else

/// @brief LSB helper that defaults to h8 on an empty board (avoids UB).
static inline square_t lsb_x(bitboard_t b)
{
    return bitboard_lsb((b | (0x8000000000000000ull)));
}

/// @brief MSB helper that defaults to a1 on an empty board (avoids UB).
static inline square_t msb_x(bitboard_t b)
{
    return bitboard_msb((b | (1ull)));
}

/// @brief Bishop attacks from @p location using classical ray subtraction (non-PEXT fallback).
static inline bitboard_t bishop_attacks(bitboard_t occupied_squares, square_t location)
{
    bitboard_t result = BISHOP_ATTACKS[location];
    result            = (result ^ NORTHEAST[lsb_x((NORTHEAST[location] & occupied_squares))]);
    result            = (result ^ NORTHWEST[lsb_x((NORTHWEST[location] & occupied_squares))]);
    result            = (result ^ SOUTHWEST[msb_x((SOUTHWEST[location] & occupied_squares))]);
    result            = (result ^ SOUTHEAST[msb_x((SOUTHEAST[location] & occupied_squares))]);
    return result;
}

/// @brief Rook attacks from @p location using classical ray subtraction (non-PEXT fallback).
static inline bitboard_t rook_attacks(bitboard_t occupied_squares, square_t location)
{
    bitboard_t result = ROOK_ATTACKS[location];
    result            = (result ^ NORTH[lsb_x((NORTH[location] & occupied_squares))]);
    result            = (result ^ SOUTH[msb_x((SOUTH[location] & occupied_squares))]);
    result            = (result ^ EAST[lsb_x((EAST[location] & occupied_squares))]);
    result            = (result ^ WEST[msb_x((WEST[location] & occupied_squares))]);
    return result;
}

#endif

/// @brief Queen attacks from square @p s (union of bishop and rook attacks).
static inline bitboard_t queen_attacks(bitboard_t occupied_squares, square_t s)
{
    return (bishop_attacks(occupied_squares, s) | rook_attacks(occupied_squares, s));
}

/// @brief Function pointer type for a piece attack generator.
typedef bitboard_t (*attack_fn_t)(bitboard_t occupied_squares, square_t locn);

/// @brief Associates a piece type with its attack-generation function.
typedef struct
{
    piece_t     piece; ///< The piece type.
    attack_fn_t fn;    ///< Attack function for this piece.
} piece_attacker_t;

/// @brief Knight attacks from square @p s (occupancy ignored).
static inline bitboard_t knight_attack_fn(bitboard_t occ, square_t s)
{
    (void)occ;
    return KNIGHT_ATTACKS[s];
}

/// @brief King attacks from square @p s (occupancy ignored).
static inline bitboard_t king_attack_fn(bitboard_t occ, square_t s)
{
    (void)occ;
    return KING_ATTACKS[s];
}

/// @brief All five piece attackers (knight, bishop, rook, queen, king).
// clang-format off
static const piece_attacker_t PIECE_ATTACKERS[5] =
{{KNIGHT, knight_attack_fn},
 {BISHOP, bishop_attacks  },
 {ROOK,   rook_attacks    },
 {QUEEN,  queen_attacks   },
 {KING,   king_attack_fn  }};

/// @brief Same as PIECE_ATTACKERS but without the king entry (used during move generation).
static const piece_attacker_t PIECE_ATTACKERS_EXCEPT_KING[4] =
{{KNIGHT, knight_attack_fn},
 {BISHOP, bishop_attacks  },
 {ROOK,   rook_attacks    },
 {QUEEN,  queen_attacks   }};
// clang-format on
