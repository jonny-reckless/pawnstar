/// @file History table implementation.

#include <stdlib.h>
#include <string.h>

#include "history_table.h"

void history_table_init(history_table_t *self)
{
    self->counts = calloc((size_t)MAX_PLY * 4096, sizeof(uint32_t));
}

void history_table_free(history_table_t *self)
{
    free(self->counts);
    self->counts = NULL;
}

void history_table_reset(history_table_t *self)
{
    memset(self->counts, 0, (size_t)MAX_PLY * 4096 * sizeof(uint32_t));
}

uint32_t history_table_max_count(const history_table_t *self)
{
    uint32_t max = 0;
    for (int i = 0; i < MAX_PLY * 4096; ++i)
    {
        if (self->counts[i] > max)
        {
            max = self->counts[i];
        }
    }
    return max;
}
