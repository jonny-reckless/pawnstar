#pragma once
/// @file Fixed-capacity stack of position_t values used to track position history during search.
#include "position.h"

/// @brief Stack of up to 256 positions (sufficient for any real game + search ply).
typedef struct
{
    position_t items[256]; ///< Position storage.
    int        size;       ///< Number of positions currently on the stack.
} position_stack_t;

/// @brief Remove all positions from the stack.
static inline void position_stack_clear(position_stack_t *s)
{
    s->size = 0;
}

/// @brief Push a copy of @p p onto the stack.
static inline void position_stack_push_back(position_stack_t *s, const position_t *p)
{
    s->items[s->size++] = *p;
}

/// @brief Pointer to the top-of-stack position (mutable).
static inline position_t *position_stack_back(position_stack_t *s)
{
    return &s->items[s->size - 1];
}

/// @brief Pointer to the top-of-stack position (read-only).
static inline const position_t *position_stack_back_const(const position_stack_t *s)
{
    return &s->items[s->size - 1];
}

/// @brief Pop the top position (does not return it).
static inline void position_stack_pop_back(position_stack_t *s)
{
    s->size--;
}

/// @brief Number of positions on the stack.
static inline int position_stack_size(const position_stack_t *s)
{
    return s->size;
}

/// @brief Pointer to the position at index @p i (mutable).
static inline position_t *position_stack_get(position_stack_t *s, int i)
{
    return &s->items[i];
}

/// @brief Pointer to the position at index @p i (read-only).
static inline const position_t *position_stack_get_const(const position_stack_t *s, int i)
{
    return &s->items[i];
}
