/// @file Functions to implement a transposition table.

#include <stdlib.h>

#include "transposition_table.h"

void transposition_table_init(transposition_table_t *self, size_t megabytes)
{
    self->size  = (megabytes * (size_t)MEGABYTE) / sizeof(transposition_t);
    self->table = calloc(self->size, sizeof(transposition_t));
    for (int i = 0; i < TT_STRIPE_COUNT; ++i)
        mtx_init(&self->stripes[i], mtx_plain);
}

void transposition_table_free(transposition_table_t *self)
{
    for (int i = 0; i < TT_STRIPE_COUNT; ++i)
        mtx_destroy(&self->stripes[i]);
    free(self->table);
    self->table = NULL;
    self->size  = 0;
}

bool transposition_table_find(transposition_table_t *self, zobrist_t hash, transposition_t *out)
{
    size_t idx = hash % self->size;
    mtx_lock(&self->stripes[idx & (TT_STRIPE_COUNT - 1)]);
    bool found = false;
    if (self->table[idx].hash == hash)
    {
        *out  = self->table[idx];
        found = true;
    }
    mtx_unlock(&self->stripes[idx & (TT_STRIPE_COUNT - 1)]);
    return found;
}

void transposition_table_record(transposition_table_t *self, const transposition_t *t)
{
    size_t idx = t->hash % self->size;
    mtx_lock(&self->stripes[idx & (TT_STRIPE_COUNT - 1)]);
    transposition_t *entry = &self->table[idx];
    if (entry->hash == 0 || entry->is_old || entry->depth <= t->depth || t->node_type == TRANSPOSITION_PV)
    {
        *entry = *t;
    }
    mtx_unlock(&self->stripes[idx & (TT_STRIPE_COUNT - 1)]);
}

void transposition_table_age(transposition_table_t *self)
{
    // Called between searches with no workers active; no locking needed.
    for (size_t i = 0; i < self->size; ++i)
        self->table[i].is_old = true;
}

void transposition_table_usage_stats(const transposition_table_t *self, size_t *count_out, int *percent_out)
{
    // Diagnostic only; no locking needed.
    size_t count = 0;
    for (size_t i = 0; i < self->size; ++i)
    {
        if (self->table[i].hash != 0)
            ++count;
    }
    *count_out   = count;
    *percent_out = (int)((count * 100) / self->size);
}
