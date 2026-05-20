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

/// @brief Return castling rights with no castling available.
static inline castling_rights_t castling_rights_default(void)
{
    return 0;
}

/// @brief Construct castling rights from a raw bitmask byte.
static inline castling_rights_t castling_rights_from_uint8(uint8_t rights)
{
    return rights;
}

/// @brief True if white may castle kingside.
static inline bool castling_rights_may_white_castle_kingside(castling_rights_t cr)
{
    return !!(cr & CASTLING_WHITE_KINGSIDE);
}
/// @brief True if white may castle queenside.
static inline bool castling_rights_may_white_castle_queenside(castling_rights_t cr)
{
    return !!(cr & CASTLING_WHITE_QUEENSIDE);
}
/// @brief True if black may castle kingside.
static inline bool castling_rights_may_black_castle_kingside(castling_rights_t cr)
{
    return !!(cr & CASTLING_BLACK_KINGSIDE);
}
/// @brief True if black may castle queenside.
static inline bool castling_rights_may_black_castle_queenside(castling_rights_t cr)
{
    return !!(cr & CASTLING_BLACK_QUEENSIDE);
}

/// @brief Zobrist hash contribution of the current castling rights.
static inline zobrist_t castling_rights_hash(castling_rights_t cr)
{
    return CASTLING_RIGHTS_HASHES[cr];
}

/// @brief Return updated castling rights after a move (identified by from/to square indices).
/// Revokes the relevant castling right whenever a king or rook moves from its starting square,
/// or when a rook's starting square is captured.
static inline castling_rights_t castling_rights_after_move(castling_rights_t cr, uint8_t from_sq, uint8_t to_sq)
{
    // clang-format off
    static const uint8_t OK = 0xFF;
    static const uint8_t MOVE_MASKS[64] =
    {
        (uint8_t)(~CASTLING_WHITE_QUEENSIDE), OK, OK, OK, (uint8_t)(~(CASTLING_WHITE_KINGSIDE|CASTLING_WHITE_QUEENSIDE)), OK, OK, (uint8_t)(~CASTLING_WHITE_KINGSIDE),
        OK, OK, OK, OK, OK, OK, OK, OK,
        OK, OK, OK, OK, OK, OK, OK, OK,
        OK, OK, OK, OK, OK, OK, OK, OK,
        OK, OK, OK, OK, OK, OK, OK, OK,
        OK, OK, OK, OK, OK, OK, OK, OK,
        OK, OK, OK, OK, OK, OK, OK, OK,
        (uint8_t)(~CASTLING_BLACK_QUEENSIDE), OK, OK, OK, (uint8_t)(~(CASTLING_BLACK_KINGSIDE|CASTLING_BLACK_QUEENSIDE)), OK, OK, (uint8_t)(~CASTLING_BLACK_KINGSIDE),
    };
    // clang-format on
    cr &= MOVE_MASKS[from_sq] & MOVE_MASKS[to_sq];
    return cr;
}

/// @brief Parse the castling field of a FEN string (e.g. @c "KQkq" or @c "-").
static inline castling_rights_t castling_rights_from_fen(const char *fen)
{
    uint8_t result = 0;
    if (fen[0] == '-' && fen[1] == '\0')
        return result;
    for (const char *p = fen; *p; ++p)
    {
        switch (*p)
        {
        case 'K':
            result |= (uint8_t)CASTLING_WHITE_KINGSIDE;
            break;
        case 'Q':
            result |= (uint8_t)CASTLING_WHITE_QUEENSIDE;
            break;
        case 'k':
            result |= (uint8_t)CASTLING_BLACK_KINGSIDE;
            break;
        case 'q':
            result |= (uint8_t)CASTLING_BLACK_QUEENSIDE;
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
        return;
    if (cr == 0)
    {
        buf[0] = '-';
        buf[1] = '\0';
        return;
    }
    int i = 0;
    if (cr & CASTLING_WHITE_KINGSIDE)
        buf[i++] = 'K';
    if (cr & CASTLING_WHITE_QUEENSIDE)
        buf[i++] = 'Q';
    if (cr & CASTLING_BLACK_KINGSIDE)
        buf[i++] = 'k';
    if (cr & CASTLING_BLACK_QUEENSIDE)
        buf[i++] = 'q';
    buf[i] = '\0';
}
