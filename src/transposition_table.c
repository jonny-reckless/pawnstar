/// @file Functions to implement a transposition table.

#include <stdlib.h>

#include "transposition_table.h"

void transposition_table_init(transposition_table_t *self, size_t megabytes)
{
    self->size  = (megabytes * (size_t)MEGABYTE) / sizeof(transposition_t);
    self->table = calloc(self->size, sizeof(transposition_t));
}

void transposition_table_free(transposition_table_t *self)
{
    free(self->table);
    self->table = NULL;
    self->size  = 0;
}

bool transposition_table_find(const transposition_table_t *self, zobrist_t hash, transposition_t *out)
{
    const transposition_t *t = &self->table[hash % self->size];
    if (t->hash == hash)
    {
        *out = *t;
        return true;
    }
    return false;
}

void transposition_table_record(transposition_table_t *self, const transposition_t *t)
{
    transposition_t *entry = &self->table[t->hash % self->size];
    if (entry->hash == 0 || entry->is_old || entry->depth <= t->depth || t->node_type == TRANSPOSITION_PV)
    {
        *entry = *t;
    }
}

void transposition_table_age(transposition_table_t *self)
{
    for (size_t i = 0; i < self->size; ++i)
        self->table[i].is_old = true;
}

void transposition_table_usage_stats(const transposition_table_t *self, size_t *count_out, int *percent_out)
{
    size_t count = 0;
    for (size_t i = 0; i < self->size; ++i)
        if (self->table[i].hash != 0)
            ++count;
    *count_out   = count;
    *percent_out = (int)((count * 100) / self->size);
}
