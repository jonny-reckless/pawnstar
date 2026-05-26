/// @file Debug counters dictionary: a small open-addressing hash table for named int64 counters.
/// Compiled in only when DEBUGX is defined. All operations are no-ops in release builds.

#pragma once
#include <stdatomic.h>
#include <stdint.h>

#if DEBUGX

#define DEBUG_TABLE_SIZE 1024 ///< Number of hash-table slots (must be a power of two).

/// @brief One slot in the debug counter table.
/// Both fields are atomic so parallel worker threads can increment without data races.
typedef struct
{
    _Atomic (const char *) key;   ///< String literal key (pointer equality used for identity after first insertion).
    _Atomic int64_t        value; ///< Current counter value.
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
static inline __attribute__((always_inline)) void debug_x_increment(const char *key)
{
    uint64_t x = (uint64_t)key;
    x >>= 2;
    x &= (DEBUG_TABLE_SIZE - 1);
    atomic_fetch_add_explicit(&debug_dictionary.buckets[x].value, 1, memory_order_seq_cst);
    atomic_store_explicit(&debug_dictionary.buckets[x].key, key, memory_order_seq_cst);
}

/// @brief Set the counter named @p key to @p val.
static inline __attribute__((always_inline)) void debug_x_assign(const char *key, int64_t val)
{
    uint64_t x = (uint64_t)key;
    x >>= 2;
    x &= (DEBUG_TABLE_SIZE - 1);
    atomic_store_explicit(&debug_dictionary.buckets[x].value, val, memory_order_seq_cst);
    atomic_store_explicit(&debug_dictionary.buckets[x].key, key, memory_order_seq_cst);
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
