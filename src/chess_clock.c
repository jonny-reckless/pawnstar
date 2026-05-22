/// @file Chess clock implementation.

#include "chess_clock.h"

void chess_clock_init(chess_clock_t *self)
{
    self->clock_type          = CLOCK_STANDARD;
    self->hard_stop_ms        = 0;
    self->ms_remaining        = 1 * 60 * 1000;
    self->num_moves_remaining = 40;
    self->depth               = 10;
    clock_gettime(CLOCK_MONOTONIC, &self->start_time);
}

int64_t chess_clock_elapsed_microseconds(const chess_clock_t *self)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (int64_t)(now.tv_sec - self->start_time.tv_sec) * 1000000 + (now.tv_nsec - self->start_time.tv_nsec) / 1000;
}
