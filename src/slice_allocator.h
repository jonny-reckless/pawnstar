#pragma once
/// @file Fixed-size slab allocator: O(1) alloc and free for a pool of same-sized objects.
#include <stddef.h>
#include <threads.h>

/// @brief A pool of @p capacity fixed-size slots. Alloc and free are O(1) and thread-safe.
typedef struct
{
    char  *pool;          ///< Heap-allocated block: capacity × object_size bytes.
    int   *free_list;     ///< Stack of available slot indices; top is at free_count − 1.
    int    free_count;    ///< Number of slots currently available.
    int    capacity;      ///< Total number of slots.
    int    max_allocated; ///< Peak number of slots simultaneously in use (watermark).
    size_t object_size;   ///< Size of each slot in bytes.
    mtx_t  lock;          ///< Protects free_list, free_count, and max_allocated.
} slice_allocator_t;

/// @brief Heap-allocate the pool and initialize the free list. Must be called once before any alloc/free.
void slice_allocator_init(slice_allocator_t *self, size_t object_size, int capacity);

/// @brief Release all pool memory. Must not call alloc/free after this.
void slice_allocator_destroy(slice_allocator_t *self);

/// @brief Claim a slot from the pool. Asserts (hard) if the pool is exhausted.
void *slice_allocator_alloc(slice_allocator_t *self);

/// @brief Return @p ptr to the pool. @p ptr must be a pointer previously returned by slice_allocator_alloc on this
/// pool.
void slice_allocator_free(slice_allocator_t *self, void *ptr);

/// @brief Return the peak number of slots simultaneously in use since the pool was initialized.
int slice_allocator_max_allocated(const slice_allocator_t *self);
