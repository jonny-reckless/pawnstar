/// @file Fixed-size slab allocator.

#include <assert.h>
#include <stdlib.h>

#include "slice_allocator.h"

/// @brief Allocate the backing pool and free_list index stack, then seed free_list with 0..capacity-1.
/// All indices are available on entry; max_allocated starts at zero.
void slice_allocator_init(slice_allocator_t *self, size_t object_size, int capacity)
{
    self->object_size = object_size;
    self->capacity    = capacity;
    self->pool        = malloc(object_size * (size_t)capacity);
    self->free_list   = malloc(sizeof(int) * (size_t)capacity);
    self->free_count    = capacity;
    self->max_allocated = 0;
    for (int i = 0; i < capacity; ++i)
        self->free_list[i] = i;
    mtx_init(&self->lock, mtx_plain);
}

/// @brief Destroy the mutex and free the pool and free_list allocations.
void slice_allocator_destroy(slice_allocator_t *self)
{
    mtx_destroy(&self->lock);
    free(self->pool);
    free(self->free_list);
}

/// @brief Pop a slot index from the free_list stack (under lock) and return the corresponding
/// object pointer.  Updates max_allocated if the current live count exceeds the previous peak.
/// Asserts that the pool is not exhausted (capacity was chosen conservatively at init time).
void *slice_allocator_alloc(slice_allocator_t *self)
{
    mtx_lock(&self->lock);
    assert(self->free_count > 0);
    int idx       = self->free_list[--self->free_count];
    int allocated = self->capacity - self->free_count;
    if (allocated > self->max_allocated)
        self->max_allocated = allocated;
    mtx_unlock(&self->lock);
    return (char *)self->pool + (size_t)idx * self->object_size;
}

/// @brief Recover the slot index from the pointer offset, then push it back onto the free_list
/// stack under the lock.  The pointer must have been returned by slice_allocator_alloc on the
/// same allocator instance.
void slice_allocator_free(slice_allocator_t *self, void *ptr)
{
    int idx = (int)(((char *)ptr - (char *)self->pool) / (ptrdiff_t)self->object_size);
    mtx_lock(&self->lock);
    self->free_list[self->free_count++] = idx;
    mtx_unlock(&self->lock);
}

/// @brief Return the peak number of simultaneously live allocations since init.
/// Read without the lock — max_allocated only ever increases, so a stale read is safe for
/// diagnostic use.
int slice_allocator_max_allocated(const slice_allocator_t *self)
{
    return self->max_allocated;
}
