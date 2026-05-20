#pragma once
/// @file Types and functions for chess moves.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "constants.h"
#include "piece.h"
#include "square.h"

/// @brief Classification of a chess move.
typedef enum
{
    MOVE_NON_CAPTURE      = 0, ///< Quiet move (no capture, no special action).
    MOVE_CAPTURE          = 1, ///< Normal capture.
    MOVE_PROMOTION_KNIGHT = 2, ///< Promotion to knight (value == KNIGHT).
    MOVE_PROMOTION_BISHOP = 3, ///< Promotion to bishop (value == BISHOP).
    MOVE_PROMOTION_ROOK   = 4, ///< Promotion to rook   (value == ROOK).
    MOVE_PROMOTION_QUEEN  = 5, ///< Promotion to queen  (value == QUEEN).
    MOVE_PAWN_DOUBLE_PUSH = 6, ///< Pawn advances two squares, setting en-passant square.
    MOVE_EP_CAPTURE       = 7, ///< En-passant capture.
    MOVE_CASTLING         = 8, ///< King-side or queen-side castling.
} move_type_t;

/// @brief A chess move packed into 64 bits.
/// Bits  0– 5: destination square
/// Bits  6–11: origin square
/// Bits 12–14: moving piece (piece_t)
/// Bits 15–17: captured piece (piece_t; NONE for non-captures)
/// Bits 18–21: move type (move_type_t)
/// Bit  22:    gives-check flag
/// Bits 32–63: sort score (signed, used by move ordering)
typedef int64_t move_t;

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

/// @brief The null move (used as a sentinel; all fields zero).
static inline move_t move_none(void)
{
    return 0;
}

/// @brief Build a move from @p from, @p to and @p piece with no special flags.
static inline move_t move_make_raw(square_t from, square_t to, piece_t piece)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12);
}

/// @brief Build a move with an explicit @p type field.
static inline move_t move_make_with_type(square_t from, square_t to, piece_t piece, move_type_t type)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12) | ((int64_t)type << 18);
}

static inline move_t move_make_capture_(square_t from, square_t to, piece_t piece, move_type_t type, piece_t captured)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12) | ((int64_t)captured << 15) |
           ((int64_t)type << 18);
}

/// @brief Quiet (non-capture) move.
static inline move_t move_non_capture(square_t from, square_t to, piece_t piece)
{
    return move_make_raw(from, to, piece);
}

/// @brief Normal capture move.
static inline move_t move_capture(square_t from, square_t to, piece_t piece, piece_t captured)
{
    return move_make_capture_(from, to, piece, MOVE_CAPTURE, captured);
}

/// @brief Promotion move, optionally with a capture (@p captured may be NONE).
static inline move_t move_promotion(square_t from, square_t to, piece_t promoted, piece_t captured)
{
    return move_make_capture_(from, to, PAWN, (move_type_t)promoted, captured);
}

/// @brief Quiet promotion (no capture).
static inline move_t move_promotion_quiet(square_t from, square_t to, piece_t promoted)
{
    return move_make_with_type(from, to, PAWN, (move_type_t)promoted);
}

/// @brief Castling move (king's from/to squares; the rook move is implicit).
static inline move_t move_castling(square_t from, square_t to)
{
    return move_make_with_type(from, to, KING, MOVE_CASTLING);
}

/// @brief En-passant capture (the captured pawn's square is derived from @p to).
static inline move_t move_ep_capture(square_t from, square_t to)
{
    return move_make_capture_(from, to, PAWN, MOVE_EP_CAPTURE, PAWN);
}

/// @brief Pawn double-push (sets en-passant square to the intermediate square).
static inline move_t move_double_push(square_t from, square_t to)
{
    return move_make_with_type(from, to, PAWN, MOVE_PAWN_DOUBLE_PUSH);
}

// ---------------------------------------------------------------------------
// String-literal square conveniences (parse "e4" at call site)
// ---------------------------------------------------------------------------

/// @brief Castling move constructed from square-name strings (e.g. @c "e1", @c "g1").
static inline move_t move_castling_str(const char *from_str, const char *to_str)
{
    return move_castling(square_from_string(from_str), square_from_string(to_str));
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

/// @brief Destination square.
static inline square_t move_to(move_t m)
{
    return (square_t)((uint8_t)(m & 0x3F));
}

/// @brief Origin square.
static inline square_t move_from(move_t m)
{
    return (square_t)((uint8_t)((m >> 6) & 0x3F));
}

/// @brief Move classification.
static inline move_type_t move_type(move_t m)
{
    return (move_type_t)((m >> 18) & 0x0F);
}

/// @brief The piece being moved.
static inline piece_t move_piece(move_t m)
{
    return (piece_t)((m >> 12) & 0x07);
}

/// @brief The piece captured (NONE if non-capture; PAWN for en-passant).
static inline piece_t move_captured(move_t m)
{
    return (piece_t)((m >> 15) & 0x07);
}

/// @brief Promotion target piece (NONE if not a promotion).
static inline piece_t move_promoted(move_t m)
{
    switch (move_type(m))
    {
    case MOVE_PROMOTION_KNIGHT:
        return KNIGHT;
    case MOVE_PROMOTION_BISHOP:
        return BISHOP;
    case MOVE_PROMOTION_ROOK:
        return ROOK;
    case MOVE_PROMOTION_QUEEN:
        return QUEEN;
    default:
        return NONE;
    }
}

/// @brief Sort score (used by move ordering; higher = searched first).
static inline int move_score(move_t m)
{
    return (int)(m >> 32);
}

/// @brief Combined from+to index (12 bits) used as a history-table key.
static inline int move_from_to(move_t m)
{
    return (int)(m & 0xFFF);
}

/// @brief True if the gives-check flag is set.
static inline bool move_is_checking(move_t m)
{
    return !!(m & (1 << 22));
}

// ---------------------------------------------------------------------------
// Mutators
// ---------------------------------------------------------------------------

/// @brief Overwrite the sort score in @p m (bits 32–63); lower bits are preserved.
static inline void move_assign_score(move_t *m, int score)
{
    *m = (*m & 0xFFFFFFFF) | ((int64_t)score << 32);
}

/// @brief Set the gives-check flag on @p m.
static inline void move_gives_check(move_t *m)
{
    *m |= (1 << 22);
}

// ---------------------------------------------------------------------------
// Comparison / hashing
// ---------------------------------------------------------------------------

/// @brief True if @p a and @p b represent the same chess move (score bits ignored).
static inline bool move_equals(move_t a, move_t b)
{
    return (a & 0x3FFFFF) == (b & 0x3FFFFF);
}

/// @brief Ordering by raw value (including score); used for sort comparators.
static inline bool move_less_than(move_t a, move_t b)
{
    return a < b;
}

/// @brief Hash of a move (for use in hash tables).
static inline size_t move_hash(move_t m)
{
    return (size_t)m;
}

// ---------------------------------------------------------------------------
// String representation (caller provides buf, at least 6 bytes)
// ---------------------------------------------------------------------------

/// @brief Write the move in UCI long-algebraic notation (e.g. "e2e4", "e7e8q") into @p buf.
void move_to_string(move_t m, char *buf, size_t buf_size);

// ---------------------------------------------------------------------------
// move_list_t — fixed-capacity stack-allocated list of moves
// ---------------------------------------------------------------------------

/// @brief A stack-allocated list of up to MAX_MOVES_PER_POSITION moves.
typedef struct
{
    move_t items[MAX_MOVES_PER_POSITION]; ///< Move storage.
    int    size;                          ///< Number of moves currently in the list.
} move_list_t;

/// @brief Remove all moves from the list.
static inline void move_list_clear(move_list_t *self)
{
    self->size = 0;
}

/// @brief Append @p m to the end of the list. No bounds check.
static inline void move_list_push_back(move_list_t *self, move_t m)
{
    self->items[self->size++] = m;
}

/// @brief Remove and return the last move.
static inline move_t move_list_pop_back(move_list_t *self)
{
    return self->items[--self->size];
}

/// @brief Pointer to the last move (not removed).
static inline move_t *move_list_back(move_list_t *self)
{
    return &self->items[self->size - 1];
}

/// @brief Return move at index @p i by value.
static inline move_t move_list_get(const move_list_t *self, int i)
{
    return self->items[i];
}

/// @brief Return a pointer to the move at index @p i.
static inline move_t *move_list_get_ptr(move_list_t *self, int i)
{
    return &self->items[i];
}

/// @brief Sort moves descending by score (unstable).
void move_list_sort(move_list_t *moves);

/// @brief Sort moves descending by score (stable — preserves relative order of equal-score moves).
void move_list_stable_sort(move_list_t *moves);

// ---------------------------------------------------------------------------
// variation_list_t — a line of moves up to MAX_PLY deep (principal variation)
// ---------------------------------------------------------------------------

/// @brief A sequence of moves up to MAX_PLY deep, used to store the principal variation.
typedef struct
{
    move_t items[MAX_PLY]; ///< Move storage.
    int    size;           ///< Number of moves in the variation.
} variation_list_t;

/// @brief Remove all moves from the variation.
static inline void variation_list_clear(variation_list_t *self)
{
    self->size = 0;
}

/// @brief Append @p m to the end of the variation.
static inline void variation_list_push_back(variation_list_t *self, move_t m)
{
    self->items[self->size++] = m;
}

/// @brief Return the move at index @p i.
static inline move_t variation_list_get(const variation_list_t *self, int i)
{
    return self->items[i];
}

/// @brief Copy @p src into @p dst, prepending @p best_move as the first entry.
static inline void copy_variation(variation_list_t *dst, const variation_list_t *src, move_t best_move)
{
    dst->size = 0;
    variation_list_push_back(dst, best_move);
    for (int i = 0; i < src->size && dst->size < MAX_PLY; i++)
        variation_list_push_back(dst, src->items[i]);
}
