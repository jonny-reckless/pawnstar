/// @file Debugging counts dictionary.

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "debug_hashtable.h"

#if DEBUGX

debug_table_t debug_dictionary;

void debug_x_clear(void)
{
    memset(&debug_dictionary, 0, sizeof(debug_dictionary));
}

typedef struct
{
    const char *key;
    int64_t     value;
} snapshot_entry_t;

static int compare_snapshot_entries(const void *a, const void *b)
{
    const snapshot_entry_t *ea = (const snapshot_entry_t *)a;
    const snapshot_entry_t *eb = (const snapshot_entry_t *)b;
    if (!ea->key)
    {
        return eb->key ? 1 : 0;
    }
    if (!eb->key)
    {
        return -1;
    }
    return strcmp(ea->key, eb->key);
}

void debug_x_write(void)
{
    snapshot_entry_t sorted[DEBUG_TABLE_SIZE];
    for (int i = 0; i < DEBUG_TABLE_SIZE; ++i)
    {
        sorted[i].key   = atomic_load_explicit(&debug_dictionary.buckets[i].key, memory_order_seq_cst);
        sorted[i].value = atomic_load_explicit(&debug_dictionary.buckets[i].value, memory_order_seq_cst);
    }
    qsort(sorted, DEBUG_TABLE_SIZE, sizeof(snapshot_entry_t), compare_snapshot_entries);

    printf("********************* DEBUGX *********************\n");
    for (int i = 0; i < DEBUG_TABLE_SIZE && sorted[i].key; ++i)
    {
        printf("%-40s%10" PRId64 "\n", sorted[i].key, sorted[i].value);
    }
    printf("**************************************************\n");
    fflush(stdout);
}

#endif
