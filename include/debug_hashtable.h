/// @file Debug counters dictionary: a small open-addressing hash table for named int64 counters.
/// Compiled in only when DEBUGX is defined. All operations are no-ops in release builds.

#pragma once
#include <stdint.h>

#if DEBUGX

#define DEBUG_TABLE_SIZE 1024 ///< Number of hash-table slots (must be a power of two).

/// @brief One slot in the debug counter table.
typedef struct
{
    const char *key;   ///< String literal key (pointer equality used for identity after first insertion).
    int64_t     value; ///< Current counter value.
} debug_entry_t;

/// @brief Open-addressing hash table of named int64 debug counters.
typedef struct
{
    debug_entry_t buckets[DEBUG_TABLE_SIZE]; ///< Fixed-size slot array.
} debug_table_t;

extern debug_table_t debug_dictionary;

/// @brief Reset all counters to zero.
void debug_x_clear(void);

/// @brief Print all non-zero counters to stdout, sorted by name.
void debug_x_write(void);

/// @brief Increment the counter named @p key by 1.
static inline void debug_x_increment(const char *key)
{
    uint64_t x = (uint64_t)key;
    x >>= 2;
    x &= (DEBUG_TABLE_SIZE - 1);
    ++debug_dictionary.buckets[x].value;
    debug_dictionary.buckets[x].key = key;
}

/// @brief Set the counter named @p key to @p val.
static inline void debug_x_assign(const char *key, int64_t val)
{
    uint64_t x = (uint64_t)key;
    x >>= 2;
    x &= (DEBUG_TABLE_SIZE - 1);
    debug_dictionary.buckets[x].value = val;
    debug_dictionary.buckets[x].key   = key;
}

#define INCREMENT(x)   debug_x_increment(x)   ///< Increment the named debug counter by 1.
#define ASSIGN(x, val) debug_x_assign(x, val) ///< Set the named debug counter to val.
#define INCREMENT_IF(b, x)        \
    do                            \
    {                             \
        if (b)                    \
            debug_x_increment(x); \
    } while (0) ///< Conditional increment.

#else

#define INCREMENT(x)
#define ASSIGN(x, val)
#define INCREMENT_IF(b, x)
#define debug_x_clear()
#define debug_x_write()

#endif
