#pragma once
/// @file Fixed-capacity stack of move_undo_t records used to track undo history during search.
#include "position.h"

/// @brief Stack of up to 256 undo records (sufficient for any real game + search ply).
typedef struct
{
    move_undo_t items[256]; ///< Undo record storage.
    int         size;       ///< Number of records currently on the stack.
} move_undo_stack_t;

/// @brief Remove all records from the stack.
static inline void move_undo_stack_clear(move_undo_stack_t *s)
{
    s->size = 0;
}

/// @brief Push @p u onto the stack.
static inline void move_undo_stack_push(move_undo_stack_t *s, const move_undo_t *u)
{
    s->items[s->size++] = *u;
}

/// @brief Pop the top record (does not return it).
static inline void move_undo_stack_pop(move_undo_stack_t *s)
{
    s->size--;
}

