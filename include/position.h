#pragma once
/// @file position_t: the complete state of a chess position.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "attacks.h"
#include "castling_rights.h"
#include "move.h"

/// @brief Complete state of a chess position.
/// Bitboards use LERF mapping (bit 0 = a1, bit 63 = h8).
/// Layout is fixed at 152 bytes for binary compatibility with saved positions.
typedef struct position_t
{
    bitboard_t        pawns;                 ///< All pawns (both colors).
    bitboard_t        knights;               ///< All knights (both colors).
    bitboard_t        bishops;               ///< All bishops (both colors).
    bitboard_t        rooks;                 ///< All rooks (both colors).
    bitboard_t        queens;                ///< All queens (both colors).
    bitboard_t        kings;                 ///< All kings (both colors).
    bitboard_t        white_pieces;          ///< All white pieces.
    bitboard_t        black_pieces;          ///< All black pieces.
    bitboard_t        checkers;              ///< Squares of pieces currently giving check to the side to move.
    zobrist_t         hash;                  ///< Zobrist hash of this position.
    piece_t           squares[64];           ///< Per-square piece type (piece_t cast to uint8_t; 0 = NONE).
    square_t          king_location[2];      ///< King square indexed by color_t (WHITE=0, BLACK=1).
    square_t          en_passant_square;     ///< En-passant target square; 0 if none available.
    uint8_t           reversible_move_count; ///< Half-move clock (fifty-move rule).
    uint8_t           full_move_count;       ///< Full-move number (incremented after Black's move).
    castling_rights_t castling_rights;       ///< Available castling options.
    uint8_t           state_flags;           ///< Bit flags: see POSITION_FLAG_*.
} position_t;

_Static_assert(sizeof(position_t) == 152, "position_t layout changed");

static const uint8_t POSITION_FLAG_BLACK_TO_MOVE = 0x01; ///< Set when it is Black's turn to move.
static const uint8_t POSITION_FLAG_NULL_MOVE     = 0x02; ///< Set when the position was reached via a null move.

// ---------------------------------------------------------------------------
// Inline accessors (read-only)
// ---------------------------------------------------------------------------

/// @brief The color whose turn it is to move.
static inline color_t position_color_to_move(const position_t *pos)
{
    return (pos->state_flags & POSITION_FLAG_BLACK_TO_MOVE) ? BLACK : WHITE;
}

/// @brief Bitboard of all occupied squares (white | black).
static inline bitboard_t position_occupied_squares(const position_t *pos)
{
    return (pos->white_pieces | pos->black_pieces);
}

/// @brief Bitboard of all unoccupied squares.
static inline bitboard_t position_vacant_squares(const position_t *pos)
{
    return (~(pos->white_pieces | pos->black_pieces));
}

/// @brief Bitboard of all pieces belonging to @p color.
static inline bitboard_t position_pieces_of_color(const position_t *pos, color_t color)
{
    return (&pos->white_pieces)[color];
}

/// @brief Bitboard of all pieces of type @p piece (both colors).
static inline bitboard_t position_pieces_of_type(const position_t *pos, piece_t piece)
{
    return (&pos->pawns)[piece - PAWN];
}

/// @brief True if the side to move is in check.
static inline bool position_is_in_check(const position_t *pos)
{
    return (pos->checkers != 0);
}

/// @brief True if the position was reached via a null move.
static inline bool position_is_null_move(const position_t *pos)
{
    return !!(pos->state_flags & POSITION_FLAG_NULL_MOVE);
}

/// @brief True if white may castle kingside.
static inline bool position_may_white_castle_kingside(const position_t *pos)
{
    return may_white_castle_kingside(pos->castling_rights);
}

/// @brief True if white may castle queenside.
static inline bool position_may_white_castle_queenside(const position_t *pos)
{
    return may_white_castle_queenside(pos->castling_rights);
}

/// @brief True if black may castle kingside.
static inline bool position_may_black_castle_kingside(const position_t *pos)
{
    return may_black_castle_kingside(pos->castling_rights);
}

/// @brief True if black may castle queenside.
static inline bool position_may_black_castle_queenside(const position_t *pos)
{
    return may_black_castle_queenside(pos->castling_rights);
}

// ---------------------------------------------------------------------------
// Attack queries
// ---------------------------------------------------------------------------

/// @brief All squares from which @p color pieces attack @p location.
static inline bitboard_t position_attacks_to(const position_t *pos, square_t location, color_t color)
{
    static const bitboard_t *enemy_pawn_attacks[2] = {PAWN_ATTACKS_BLACK, PAWN_ATTACKS_WHITE};
    const bitboard_t         occupied              = pos->white_pieces | pos->black_pieces;
    bitboard_t               result                = enemy_pawn_attacks[color][location] & pos->pawns;
    result |= (KNIGHT_ATTACKS[location] & pos->knights) | (KING_ATTACKS[location] & pos->kings);
    result |= (rook_attacks(occupied, location) & (pos->rooks | pos->queens));
    result |= (bishop_attacks(occupied, location) & (pos->bishops | pos->queens));
    return (result & position_pieces_of_color(pos, color));
}

/// @brief True if square @p s is attacked by at least one piece of @p c.
static inline bool position_is_attacked(const position_t *pos, square_t s, color_t c)
{
    return position_attacks_to(pos, s, c) != 0;
}

// ---------------------------------------------------------------------------
// Non-inline operations (defined in position.c)
// ---------------------------------------------------------------------------

/// @brief Parse a FEN string and return the corresponding position.
position_t position_from_string(const char *fen_string);

/// @brief Return the position that results from playing @p move (does not modify @p pos).
position_t position_make_move(const position_t *pos, move_t move);

/// @brief Return the position after a null move (passes the turn; used in null-move pruning).
position_t position_make_null_move(const position_t *pos);

/// @brief Serialize the position to a FEN string in @p buf.
void position_to_string(const position_t *pos, char *buf, size_t buf_size);

/// @brief True if the position is a draw by insufficient material.
bool position_is_draw_by_material(const position_t *pos);

/// @brief True if the side to move is checkmated.
bool position_is_checkmate(const position_t *pos);

/// @brief True if the side to move is stalemated.
bool position_is_stalemate(const position_t *pos);

/// @brief Generate all legal moves for the side to move.
move_list_t position_generate_legal_moves(const position_t *pos);

/// @brief Generate only legal capture moves for the side to move (used by quiescence search).
move_list_t position_generate_legal_captures(const position_t *pos);
