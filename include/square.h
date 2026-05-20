#pragma once
/// @file square_t indices on the chess board.
#include <stddef.h>
#include <stdint.h>

/// @brief A square index (location) on a chess board in LERF mapping.
typedef uint8_t square_t;

/// @brief Construct from file and rank.
static inline square_t square_from_coords(int file, int rank)
{
    return (square_t)(file + 8 * rank);
}

/// @brief Construct from name string e.g. "e4".
static inline square_t square_from_string(const char *str)
{
    return (square_t)(((str[0] | 0x20) - 'a') + 8 * (str[1] - '1'));
}

/// @brief File (0..7 → a..h).
static inline int square_file(square_t s)
{
    return s & 7;
}

/// @brief Rank (0..7).
static inline int square_rank(square_t s)
{
    return s >> 3;
}

/// @brief Write square name (e.g. "e4") into buf; buf must be at least 3 bytes.
static inline void square_to_string(square_t s, char *buf, size_t buf_size)
{
    if (buf_size >= 3)
    {
        buf[0] = (char)('a' + square_file(s));
        buf[1] = (char)('1' + square_rank(s));
        buf[2] = '\0';
    }
}
