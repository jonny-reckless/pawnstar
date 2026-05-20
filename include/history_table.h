#pragma once
/// @file History heuristic table: tracks which moves have historically raised alpha or caused cutoffs.
#include <stdint.h>
#include <string.h>

#include "constants.h"
#include "move.h"

/// @brief Per-ply history counts indexed by the combined from+to square index of each move.
/// Counts are accumulated across the search and used to bias move ordering.
typedef struct
{
    uint32_t *counts; ///< Heap-allocated array of size MAX_PLY × 4096 (one entry per ply × from+to pair).
} history_table_t;

/// @brief Allocate and zero-initialize the history table.
void history_table_init(history_table_t *self);

/// @brief Free the underlying allocation.
void history_table_free(history_table_t *self);

/// @brief Zero all counts (called at the start of each search).
void history_table_reset(history_table_t *self);

/// @brief Return the largest count stored across all entries.
uint32_t history_table_max_count(const history_table_t *self);

/// @brief Increment the count for @p move at @p ply (called when a move raises alpha or causes a cutoff).
static inline void history_table_record_good_move(history_table_t *self, int ply, move_t move)
{
    ++self->counts[ply * 4096 + move_from_to(move)];
}

/// @brief Return the count for @p move at @p ply.
static inline uint32_t history_table_get_count(const history_table_t *self, int ply, move_t move)
{
    return self->counts[ply * 4096 + move_from_to(move)];
}
