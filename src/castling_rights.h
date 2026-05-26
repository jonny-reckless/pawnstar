#pragma once
/// @file Castling-rights representation for a chess position.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constants.h"

/// Forward declaration — defined in generated_data.c.
extern const zobrist_t CASTLING_RIGHTS_HASHES[16];

/// @brief Castling availability for both sides.
/// A 4-bit bitmask (CASTLING_* flags) that doubles as an index
/// into CASTLING_RIGHTS_HASHES[0..15].
typedef uint8_t castling_rights_t;

/// @brief Individual castling-right bits (used as bitmask flags in castling_rights_t).
enum CastlingFlags
{
    CASTLING_WHITE_KINGSIDE  = 0x01, ///< White may castle kingside.
    CASTLING_WHITE_QUEENSIDE = 0x02, ///< White may castle queenside.
    CASTLING_BLACK_KINGSIDE  = 0x04, ///< Black may castle kingside.
    CASTLING_BLACK_QUEENSIDE = 0x08, ///< Black may castle queenside.
};

/// @brief Zobrist hash contribution of the current castling rights.
static inline zobrist_t castling_rights_hash(castling_rights_t cr)
{
    return CASTLING_RIGHTS_HASHES[cr];
}

/// @brief Return updated castling rights after a move (identified by from/to square indices).
/// Revokes the relevant castling right whenever a king or rook moves from its starting square,
/// or when a rook's starting square is captured.
static inline castling_rights_t castling_rights_after_move(castling_rights_t cr, square_t from_sq, square_t to_sq)
{
    // clang-format off
    static const castling_rights_t MOVE_MASKS[64] =
    {
        // Only squares affecting castle rights are a1, e1, h1, a8, e8, h8
        0x0D, 0x0F, 0x0F, 0x0F, 0x0C, 0x0F, 0x0F, 0x0E,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F, 0x0F,
        0x07, 0x0F, 0x0F, 0x0F, 0x03, 0x0F, 0x0F, 0x0B,
    };
    // clang-format on
    return cr & MOVE_MASKS[from_sq] & MOVE_MASKS[to_sq];
}

/// @brief Parse the castling field of a FEN string (e.g. @c "KQkq" or @c "-").
static inline castling_rights_t castling_rights_from_fen(const char *fen)
{
    castling_rights_t result = 0;
    if (fen[0] == '-')
    {
        return result;
    }
    for (const char *p = fen; *p; ++p)
    {
        switch (*p)
        {
        case 'K':
            result |= CASTLING_WHITE_KINGSIDE;
            break;
        case 'Q':
            result |= CASTLING_WHITE_QUEENSIDE;
            break;
        case 'k':
            result |= CASTLING_BLACK_KINGSIDE;
            break;
        case 'q':
            result |= CASTLING_BLACK_QUEENSIDE;
            break;
        default:
            break;
        }
    }
    return result;
}

/// @brief Serialize castling rights to a FEN string (e.g. @c "KQkq"); @p buf must be at least 5 bytes.
static inline void castling_rights_to_fen_string(castling_rights_t cr, char *buf, size_t buf_size)
{
    if (buf_size < 5)
    {
        return;
    }
    if (cr == 0)
    {
        *buf++ = '-';
        *buf   = '\0';
        return;
    }
    if (cr & CASTLING_WHITE_KINGSIDE)
    {
        *buf++ = 'K';
    }
    if (cr & CASTLING_WHITE_QUEENSIDE)
    {
        *buf++ = 'Q';
    }
    if (cr & CASTLING_BLACK_KINGSIDE)
    {
        *buf++ = 'k';
    }
    if (cr & CASTLING_BLACK_QUEENSIDE)
    {
        *buf++ = 'q';
    }
    *buf = '\0';
}
