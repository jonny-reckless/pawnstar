#pragma once
/// @file Types and functions for chess moves.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "color.h"
#include "constants.h"
#include "piece.h"
#include "square.h"

/// @brief Move class. The four promotion values equal the promoted piece_t so they can be cast directly.
typedef enum
{
    MOVE_NON_CAPTURE      = 0, ///< Quiet move.
    MOVE_CAPTURE          = 1, ///< Normal capture.
    MOVE_PROMOTION_KNIGHT = 2, ///< Promotion to knight.
    MOVE_PROMOTION_BISHOP = 3, ///< Promotion to bishop.
    MOVE_PROMOTION_ROOK   = 4, ///< Promotion to rook.
    MOVE_PROMOTION_QUEEN  = 5, ///< Promotion to queen.
    MOVE_PAWN_DOUBLE_PUSH = 6, ///< Pawn advances two squares; sets the en-passant target square.
    MOVE_EP_CAPTURE       = 7, ///< En-passant capture.
    MOVE_CASTLING         = 8, ///< King-side or queen-side castling.
} move_type_t;

/// @brief A chess move packed into one 64-bit word.
/// Bits  0– 5: destination square
/// Bits  6–11: origin square
/// Bits 12–14: moving piece (piece_t)
/// Bits 15–17: captured piece (piece_t; NONE for quiet moves)
/// Bits 18–21: move type (move_type_t)
/// Bit  22:    gives-check flag
/// Bits 32–63: move-ordering score (signed)
typedef int64_t move_t;

/// @brief Sentinel meaning "no move"; all fields zero.
static inline move_t move_none(void)
{
    return 0;
}

/// @brief Build a non-capture move with an explicit type (e.g. castling, double push, quiet promotion).
static inline __attribute__((always_inline)) move_t move_make_with_type(square_t from, square_t to, piece_t piece,
                                                                        move_type_t type)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12) | ((int64_t)type << 18);
}

/// @brief Internal builder: pack all five move fields into a move_t.
static inline __attribute__((always_inline)) move_t move_make_capture_(square_t from, square_t to, piece_t piece,
                                                                       move_type_t type, piece_t captured)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12) | ((int64_t)captured << 15) |
           ((int64_t)type << 18);
}

/// @brief Quiet move (no capture, no special flag).
static inline __attribute__((always_inline)) move_t move_non_capture(square_t from, square_t to, piece_t piece)
{
    return (int64_t)to | ((int64_t)from << 6) | ((int64_t)piece << 12);
}

/// @brief Normal capture.
static inline __attribute__((always_inline)) move_t move_capture(square_t from, square_t to, piece_t piece,
                                                                 piece_t captured)
{
    return move_make_capture_(from, to, piece, MOVE_CAPTURE, captured);
}

/// @brief Promotion, optionally with a capture (@p captured may be NONE).
static inline __attribute__((always_inline)) move_t move_promotion(square_t from, square_t to, piece_t promoted,
                                                                   piece_t captured)
{
    return move_make_capture_(from, to, PAWN, (move_type_t)promoted, captured);
}

/// @brief Quiet promotion (pawn reaches the back rank without capturing).
static inline __attribute__((always_inline)) move_t move_promotion_quiet(square_t from, square_t to, piece_t promoted)
{
    return move_make_with_type(from, to, PAWN, (move_type_t)promoted);
}

/// @brief Castling move; the rook's path is derived from the king's @p to square.
static inline __attribute__((always_inline)) move_t move_castling(square_t from, square_t to)
{
    return move_make_with_type(from, to, KING, MOVE_CASTLING);
}

/// @brief En-passant capture; the captured pawn's square is derived from @p to.
static inline __attribute__((always_inline)) move_t move_ep_capture(square_t from, square_t to)
{
    return move_make_capture_(from, to, PAWN, MOVE_EP_CAPTURE, PAWN);
}

/// @brief Pawn double-push; the en-passant target square is set to the skipped square.
static inline __attribute__((always_inline)) move_t move_double_push(square_t from, square_t to)
{
    return move_make_with_type(from, to, PAWN, MOVE_PAWN_DOUBLE_PUSH);
}

// ---------------------------------------------------------------------------
// Accessors
// ---------------------------------------------------------------------------

/// @brief Destination square (bits 0–5).
static inline __attribute__((always_inline)) square_t move_to(move_t m)
{
    return (square_t)(m & 0x3F);
}

/// @brief Origin square (bits 6–11).
static inline __attribute__((always_inline)) square_t move_from(move_t m)
{
    return (square_t)((m >> 6) & 0x3F);
}

/// @brief Move classification (bits 18–21).
static inline __attribute__((always_inline)) move_type_t move_type(move_t m)
{
    return (move_type_t)((m >> 18) & 0x0F);
}

/// @brief Piece being moved (bits 12–14).
static inline __attribute__((always_inline)) piece_t move_piece(move_t m)
{
    return (piece_t)((m >> 12) & 0x07);
}

/// @brief Piece captured; NONE for quiet moves, PAWN for en-passant (bits 15–17).
static inline __attribute__((always_inline)) piece_t move_captured(move_t m)
{
    return (piece_t)((m >> 15) & 0x07);
}

/// @brief Promotion target; NONE if the move is not a promotion.
static inline __attribute__((always_inline)) piece_t move_promoted(move_t m)
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

/// @brief Move-ordering score (bits 32–63); higher scores are searched first.
static inline __attribute__((always_inline)) int move_score(move_t m)
{
    return (int)(m >> 32);
}

/// @brief Combined origin+destination index (bits 0–11); used as a history-table key.
static inline __attribute__((always_inline)) int move_from_to(move_t m)
{
    return (int)(m & 0xFFF);
}

// ---------------------------------------------------------------------------
// Mutators
// ---------------------------------------------------------------------------

/// @brief Set the move-ordering score; bits 0–31 (the move fields) are unchanged.
static inline __attribute__((always_inline)) void move_assign_score(move_t *m, int score)
{
    *m = (*m & 0xFFFFFFFF) | ((int64_t)score << 32);
}

/// @brief Mark @p m as giving check (sets bit 22).
static inline __attribute__((always_inline)) void move_gives_check(move_t *m)
{
    *m |= (1 << 22);
}

// ---------------------------------------------------------------------------
// Comparison
// ---------------------------------------------------------------------------

/// @brief True if @p a and @p b encode the same move, ignoring their sort scores.
static inline __attribute__((always_inline)) bool move_equals(move_t a, move_t b)
{
    return (a & 0x3FFFFF) == (b & 0x3FFFFF);
}

// ---------------------------------------------------------------------------
// String representation (caller provides buf, at least 6 bytes)
// ---------------------------------------------------------------------------

/// @brief Write @p m in UCI long-algebraic notation (e.g. "e2e4", "e7e8q") into @p buf.
void move_to_string(move_t m, char *buf, size_t buf_size);

// ---------------------------------------------------------------------------
// move_list_t — fixed-capacity stack-allocated list of moves
// ---------------------------------------------------------------------------

/// @brief Stack-allocated array of up to MAX_MOVES_PER_POSITION moves.
typedef struct
{
    move_t items[MAX_MOVES_PER_POSITION]; ///< Move storage.
    int    size;                          ///< Number of moves currently in the list.
} move_list_t;

/// @brief Append @p m to @p self. UB if the list is already at capacity.
static inline __attribute__((always_inline)) void move_list_push_back(move_list_t *self, move_t m)
{
    self->items[self->size++] = m;
}

/// @brief Sort @p moves descending by score; equal-score order is unspecified.
void move_list_sort(move_list_t *moves);

/// @brief Sort @p moves descending by score, preserving the relative order of equal-score moves.
void move_list_stable_sort(move_list_t *moves);

// ---------------------------------------------------------------------------
// variation_t — a line of moves up to MAX_PLY deep (principal variation)
// ---------------------------------------------------------------------------

/// @brief A line of up to MAX_PLY moves, used to record the principal variation.
typedef struct
{
    move_t items[MAX_PLY]; ///< Move storage.
    int    size;           ///< Number of moves currently in the variation.
} variation_t;

/// @brief Discard all moves from @p self.
static inline void variation_clear(variation_t *self)
{
    self->size = 0;
}

/// @brief Append @p m to @p self.
static inline void variation_push_back(variation_t *self, move_t m)
{
    self->items[self->size++] = m;
}

/// @brief Set @p dst to @p best_move followed by all moves in @p src (truncated at MAX_PLY).
static inline void variation_copy(variation_t *dst, const variation_t *src, move_t best_move)
{
    dst->size = 0;
    variation_push_back(dst, best_move);
    for (int i = 0; i < src->size && dst->size < MAX_PLY; i++)
    {
        variation_push_back(dst, src->items[i]);
    }
}
