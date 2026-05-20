/// @file Debugging counts dictionary.

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

static int compare_debug_entries(const void *a, const void *b)
{
    const debug_entry_t *ea = (const debug_entry_t *)a;
    const debug_entry_t *eb = (const debug_entry_t *)b;
    if (!ea->occupied)
    {
        return eb->occupied ? 1 : 0;
    }
    if (!eb->occupied)
    {
        return -1;
    }
    return strcmp(ea->key, eb->key);
}

void debug_x_write(void)
{
    debug_entry_t sorted[DEBUG_TABLE_SIZE];
    memcpy(sorted, debug_dictionary.buckets, sizeof(sorted));
    qsort(sorted, DEBUG_TABLE_SIZE, sizeof(debug_entry_t), compare_debug_entries);

    printf("********************* DEBUGX *********************\n");
    for (int i = 0; i < DEBUG_TABLE_SIZE && sorted[i].occupied; ++i)
    {
        printf("%-40s%10lld\n", sorted[i].key, (long long)sorted[i].value);
    }
    printf("**************************************************\n");
    fflush(stdout);
}

#endif
