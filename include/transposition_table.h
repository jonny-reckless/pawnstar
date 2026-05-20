#pragma once
/// @file Transposition table: caches alpha-beta search results for previously seen positions.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "constants.h"
#include "move.h"

/// @brief Classification of a transposition table entry by the search result type.
typedef enum
{
    TRANSPOSITION_CUT = 0, ///< Beta cutoff: score is a lower bound.
    TRANSPOSITION_ALL = 1, ///< All-node: no move exceeded alpha; score is an upper bound.
    TRANSPOSITION_PV  = 2, ///< PV-node: exact score within the search window.
} transposition_node_type_t;

/// @brief A single transposition table entry (24 bytes).
typedef struct
{
    zobrist_t hash;      ///< Zobrist hash of the stored position.
    move_t    move;      ///< Best (or refutation) move found from this position.
    int       score;     ///< Score computed at the given depth.
    int16_t   depth;     ///< Search depth at which this entry was written.
    uint8_t   node_type; ///< transposition_node_type_t stored as a byte.
    bool      is_old;    ///< Entry was written during a previous search and may be evicted.
} transposition_t;

_Static_assert(sizeof(transposition_t) == 24, "transposition_t layout changed");

/// @brief The transposition table: a fixed-size heap-allocated hash table.
/// Indexed by hash % size; collisions overwrite the existing entry.
typedef struct
{
    transposition_t *table; ///< Heap-allocated array of entries.
    size_t           size;  ///< Number of entries.
} transposition_table_t;

/// @brief Allocate a transposition table of @p megabytes.
void transposition_table_init(transposition_table_t *self, size_t megabytes);

/// @brief Free the table's memory.
void transposition_table_free(transposition_table_t *self);

/// @brief Look up @p hash in the table. Returns true and fills @p out if found.
bool transposition_table_find(const transposition_table_t *self, zobrist_t hash, transposition_t *out);

/// @brief Write entry @p t into the table (may overwrite an existing entry).
void transposition_table_record(transposition_table_t *self, const transposition_t *t);

/// @brief Mark all entries as old so they may be replaced by the next search.
void transposition_table_age(transposition_table_t *self);

/// @brief Return the number of occupied entries and fill percentage in @p count_out / @p percent_out.
void transposition_table_usage_stats(const transposition_table_t *self, size_t *count_out, int *percent_out);
