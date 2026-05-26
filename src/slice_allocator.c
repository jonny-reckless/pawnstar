/// @file Fixed-size slab allocator.

#include <assert.h>
#include <stdlib.h>

#include "slice_allocator.h"

void slice_allocator_init(slice_allocator_t *self, size_t object_size, int capacity)
{
    self->object_size = object_size;
    self->capacity    = capacity;
    self->pool        = malloc(object_size * (size_t)capacity);
    self->free_list   = malloc(sizeof(int) * (size_t)capacity);
    self->free_count  = capacity;
    for (int i = 0; i < capacity; ++i)
        self->free_list[i] = i;
    mtx_init(&self->lock, mtx_plain);
}

void slice_allocator_destroy(slice_allocator_t *self)
{
    mtx_destroy(&self->lock);
    free(self->pool);
    free(self->free_list);
}

void *slice_allocator_alloc(slice_allocator_t *self)
{
    mtx_lock(&self->lock);
    assert(self->free_count > 0);
    int idx = self->free_list[--self->free_count];
    mtx_unlock(&self->lock);
    return (char *)self->pool + (size_t)idx * self->object_size;
}

void slice_allocator_free(slice_allocator_t *self, void *ptr)
{
    int idx = (int)(((char *)ptr - (char *)self->pool) / (ptrdiff_t)self->object_size);
    mtx_lock(&self->lock);
    self->free_list[self->free_count++] = idx;
    mtx_unlock(&self->lock);
}
