#pragma once
/// @file Chess clock: tracks elapsed time and enforces time-control limits.
#include <stdint.h>
#include <time.h>

/// @brief Time control mode for the current game.
typedef enum
{
    CLOCK_STANDARD,    ///< N moves must be made within a total time budget.
    CLOCK_FIXED_DEPTH, ///< Search to a fixed depth on every move.
    CLOCK_FIXED_TIME,  ///< Search for a fixed number of milliseconds on every move.
    CLOCK_INFINITE,    ///< Search indefinitely until a UCI @c stop command arrives.
} clock_type_t;

/// @brief Tracks elapsed time and time-control parameters for one game.
typedef struct
{
    clock_type_t    clock_type;          ///< Active time control mode.
    int64_t         hard_stop_ms;        ///< Absolute deadline in elapsed milliseconds; search must stop by this time.
    int             ms_remaining;        ///< Milliseconds remaining in the current time period.
    int             num_moves_remaining; ///< Moves remaining before the next time control (CLOCK_STANDARD only).
    int             depth;               ///< Fixed search depth (CLOCK_FIXED_DEPTH only).
    struct timespec start_time;          ///< Monotonic timestamp when the clock was last started.
} chess_clock_t;

/// @brief Initialize the clock with CLOCK_INFINITE and record the start time.
void chess_clock_init(chess_clock_t *self);

/// @brief Return the number of microseconds elapsed since the clock was started.
int64_t chess_clock_elapsed_microseconds(const chess_clock_t *self);

/// @brief Return the number of milliseconds elapsed since the clock was started.
static inline int64_t chess_clock_elapsed_milliseconds(const chess_clock_t *self)
{
    return chess_clock_elapsed_microseconds(self) / 1000;
}
