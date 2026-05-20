#pragma once
/// @file Chess piece colors.
#include <stdint.h>

/// @brief piece_t colors.
typedef enum
{
    WHITE = 0,
    BLACK = 1,
} color_t;

/// @brief Return the enemy of a color.
static inline color_t enemy_of(color_t color)
{
    return color == WHITE ? BLACK : WHITE;
}
