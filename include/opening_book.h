#pragma once
/// @file Opening book: maps Zobrist hashes to the set of moves playable from each known position.
#include <stddef.h>
#include <stdint.h>

#include "move.h"

typedef struct position_t position_t;

/// @brief One entry in the opening book: a position hash and the moves available from it.
typedef struct
{
    zobrist_t key;        ///< Zobrist hash of the position.
    move_t   *moves;      ///< Heap-allocated array of book moves.
    int       move_count; ///< Number of moves in the array.
    int       move_cap;   ///< Allocated capacity of the moves array.
} book_entry_t;

/// @brief The complete opening book.
/// During initialization entries are accumulated then sorted by key for O(log n) lookup via bsearch.
typedef struct
{
    book_entry_t *entries;        ///< Array of entries, sorted by key after initialization.
    size_t        entry_count;    ///< Number of entries currently stored.
    size_t        entry_capacity; ///< Allocated capacity of the entries array.
    uint64_t      prng_state;     ///< xorshift64 seed for random move selection.
} opening_book_t;

/// @brief Initialize an empty opening book (no memory allocated yet).
void opening_book_init(opening_book_t *self);

/// @brief Load opening lines from @p filename (plain-text format, one line of play per line).
/// Returns true on success.
bool opening_book_initialize(opening_book_t *self, const char *filename);

/// @brief Print all book moves available for @p position to stdout.
void opening_book_display_available_moves(const opening_book_t *self, const struct position_t *position);

/// @brief Return a randomly-selected book move for position @p hash, or move_none() if not in book.
move_t opening_book_get_move(opening_book_t *self, zobrist_t hash);

/// @brief Free all heap memory owned by the opening book.
void opening_book_free(opening_book_t *self);
