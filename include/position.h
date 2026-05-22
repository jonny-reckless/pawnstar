#pragma once
/// @file position_t: the complete state of a chess position.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "attacks.h"
#include "castling_rights.h"
#include "move.h"

typedef uint8_t pos_flags_t;

static const pos_flags_t POSITION_FLAG_BLACK_TO_MOVE = 0x01; ///< Set when it is Black's turn to move.
static const pos_flags_t POSITION_FLAG_NULL_MOVE     = 0x02; ///< Set when the position was reached via a null move.

/// @brief Irreversible state saved before a move so it can be undone.
/// Only the fields that cannot be reconstructed from the move itself are saved.
typedef struct
{
    zobrist_t         hash;                  ///< Zobrist hash before the move.
    bitboard_t        checkers;              ///< Checker bitboard before the move.
    square_t          en_passant_square;     ///< En-passant target before the move.
    castling_rights_t castling_rights;       ///< Castling rights before the move.
    pos_flags_t       flags;                 ///< State flags before the move.
    uint8_t           reversible_move_count; ///< Half-move clock before the move.
    uint8_t           full_move_count;       ///< Full-move counter before the move.
} move_undo_t;

/// @brief Stack of up to 256 undo records (sufficient for any real game + search ply).
typedef struct
{
    move_undo_t items[256]; ///< Undo record storage.
    int         count;      ///< Number of records currently on the stack.
} move_undo_stack_t;

/// @brief Remove all records from the stack.
static inline void move_undo_stack_clear(move_undo_stack_t *s)
{
    s->count = 0;
}

/// @brief Push @p u onto the stack.
static inline void move_undo_stack_push(move_undo_stack_t *s, const move_undo_t *u)
{
    s->items[s->count++] = *u;
}

/// @brief Pop the top record (does not return it).
static inline void move_undo_stack_pop(move_undo_stack_t *s)
{
    s->count--;
}

/// @brief Complete state of a chess position.
/// Bitboards use LERF mapping (bit 0 = a1, bit 63 = h8).
typedef struct position_t
{
    bitboard_t  pawns;            ///< All pawns (both colors).
    bitboard_t  knights;          ///< All knights (both colors).
    bitboard_t  bishops;          ///< All bishops (both colors).
    bitboard_t  rooks;            ///< All rooks (both colors).
    bitboard_t  queens;           ///< All queens (both colors).
    bitboard_t  kings;            ///< All kings (both colors).
    bitboard_t  white_pieces;     ///< All white pieces.
    bitboard_t  black_pieces;     ///< All black pieces.
    piece_t     squares[64];      ///< Per-square piece type (piece_t cast to uint8_t; 0 = NONE).
    move_undo_t state;            ///< Reversible state snapshot (hash, checkers, ep square, etc.).
    square_t    king_location[2]; ///< King square indexed by color_t (WHITE=0, BLACK=1).
} position_t;

_Static_assert(sizeof(position_t) == 160, "position_t layout changed");

// ---------------------------------------------------------------------------
// Inline accessors (read-only)
// ---------------------------------------------------------------------------

/// @brief The color whose turn it is to move.
static inline color_t position_color_to_move(const position_t *pos)
{
    return (pos->state.flags & POSITION_FLAG_BLACK_TO_MOVE) ? BLACK : WHITE;
}

/// @brief Bitboard of all occupied squares (white | black).
static inline bitboard_t position_occupied_squares(const position_t *pos)
{
    return pos->white_pieces | pos->black_pieces;
}

/// @brief Bitboard of all unoccupied squares.
static inline bitboard_t position_vacant_squares(const position_t *pos)
{
    return ~(pos->white_pieces | pos->black_pieces);
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
    return pos->state.checkers != 0;
}

/// @brief True if the position was reached via a null move.
static inline bool position_is_null_move(const position_t *pos)
{
    return !!(pos->state.flags & POSITION_FLAG_NULL_MOVE);
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
    result |= rook_attacks(occupied, location) & (pos->rooks | pos->queens);
    result |= bishop_attacks(occupied, location) & (pos->bishops | pos->queens);
    return result & position_pieces_of_color(pos, color);
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

/// @brief Apply @p move to @p pos in place. Saves irreversible state in @p undo.
void position_make_move(position_t *pos, move_t move, move_undo_t *undo);

/// @brief Reverse @p move on @p pos using the previously saved @p undo record.
void position_undo_move(position_t *pos, move_t move, const move_undo_t *undo);

/// @brief Apply a null move (pass the turn) to @p pos in place. Saves state in @p undo.
void position_make_null_move(position_t *pos, move_undo_t *undo);

/// @brief Reverse a null move using the previously saved @p undo record.
void position_undo_null_move(position_t *pos, const move_undo_t *undo);

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
