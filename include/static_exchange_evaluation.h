#pragma once
/// @file Static exchange evaluation: estimates net material gain/loss of a capture sequence.
#include "move.h"
#include <stdbool.h>

typedef struct position_t position_t;

/// @brief Result of a static exchange evaluation.
typedef struct
{
    int  score;       ///< Net material gained (positive) or lost (negative) after the full capture sequence.
    bool is_checking; ///< True if the initiating move gives check.
} see_result_t;

/// @brief Evaluate the full capture sequence starting with @p move on @p src_position.
/// Returns the net material score and whether the first move gives check.
see_result_t evaluate_static_exchange(const position_t *src_position, move_t move);
